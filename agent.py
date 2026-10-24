import pexpect
import json
import urllib.request
import sys

# 1. Host-to-QEMU Serial Bridge (Persists across commands)
class Xv6Bridge:
    def __init__(self):
        print("[Bridge] Spawning xv6 in QEMU...", flush=True)
        self.child = pexpect.spawn("make TOOLPREFIX=riscv64-linux-gnu- qemu", encoding='utf-8', timeout=10)
        self.child.expect(r'\$ ')
        print("[Bridge] xv6 ready.\n", flush=True)

    def run_cmd(self, cmd):
        print(f"[Bridge Execution] -> sending: {cmd}", flush=True)
        self.child.sendline(cmd)
        try:
            self.child.expect(r'\$ ')
            output = self.child.before.replace('\r', '').split('\n')
            return '\n'.join(output[1:]).strip()
        except pexpect.TIMEOUT:
            return "Error: Command timed out."

    def close(self):
        print("[Bridge] Shutting down QEMU...", flush=True)
        self.child.sendline("\x01x")

# 2. Host Match-Action Table Validator
MATCH_ACTION_TABLE = {
    "hello": {
        "required_args": ["a", "b", "c", "d"],
        "action": lambda args: f"hello {args['a']} {args['b']} {args['c']} {args['d']}"
    },
    "execute_effect": {
        "required_args": ["key", "op", "path", "buf"],
        "action": lambda args: f"effect {args['key']} {args['op']} {args['path']} \"{args['buf']}\""
    }
}

def evaluate_host_match_action(tool_call):
    tool_name = tool_call.get("tool")
    args = tool_call.get("args", {})

    if tool_name not in MATCH_ACTION_TABLE:
        print(f"[Host Match-Action] MATCH FAIL: Unknown tool '{tool_name}'", flush=True)
        return False, None

    rule = MATCH_ACTION_TABLE[tool_name]
    for req_arg in rule["required_args"]:
        if req_arg not in args:
            print(f"[Host Match-Action] MATCH FAIL: Missing arg '{req_arg}'", flush=True)
            return False, None

    cmd = rule["action"](args)
    print(f"[Host Match-Action] MATCH SUCCESS: Generated command -> {cmd}", flush=True)
    return True, cmd

# 3. LLM Request
def ask_llm(user_prompt):
    print(f"\n--- Querying LLM: \"{user_prompt}\" ---", flush=True)
    system_instruction = """
    You are an AI assistant. Return strictly valid JSON matching one of these tool schemas:
    1. {"tool": "hello", "args": {"a": int, "b": int, "c": int, "d": int}}
    2. {"tool": "execute_effect", "args": {"key": int, "op": int, "path": string, "buf": string}}
    Do not output any markdown formatting or prose.
    """
    data = json.dumps({
        "model": "llama3.1",
        "prompt": f"{system_instruction}\nUser Goal: {user_prompt}",
        "format": "json",
        "stream": False
    }).encode('utf-8')

    req = urllib.request.Request("http://host.lima.internal:11434/api/generate", data=data)
    req.add_header('Content-Type', 'application/json')

    try:
        with urllib.request.urlopen(req, timeout=20) as response:
            res = json.loads(response.read().decode('utf-8'))
            return json.loads(res['response'])
    except Exception as e:
        print(f"[LLM Error]: {e}", flush=True)
        return None

def process_goal(bridge, user_query):
    proposed_action = ask_llm(user_query)
    if proposed_action is None:
        return

    print(f"[LLM Output]: {json.dumps(proposed_action)}", flush=True)
    matched, command = evaluate_host_match_action(proposed_action)

    if matched:
        obs = bridge.run_cmd(command)
        print(f"\n[xv6 Observation]:\n{obs}\n", flush=True)
    else:
        print("[Host Match-Action] Action rejected before executing on QEMU.", flush=True)

if __name__ == "__main__":
    initial_query = sys.argv[1] if len(sys.argv) > 1 else "Store record with key 9001 and text ItemA to /orders.txt"

    # Start QEMU ONCE
    bridge = Xv6Bridge()

    # Run 1: First Execution
    print("=== RUN 1: First Execution ===", flush=True)
    process_goal(bridge, initial_query)

    # Run 2: Replay Execution (Same session -> Tests in-memory table lookup)
    print("=== RUN 2: Replay Attempt (Same Key) ===", flush=True)
    process_goal(bridge, initial_query)

    bridge.close()
