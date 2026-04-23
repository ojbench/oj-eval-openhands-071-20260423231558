#!/usr/bin/env python3
import requests
import os
import json

access_token = os.environ.get("ACMOJ_TOKEN")
api_base = "https://acm.sjtu.edu.cn/OnlineJudge/api/v1"
headers = {
    "Authorization": f"Bearer {access_token}",
    "Content-Type": "application/x-www-form-urlencoded",
    "User-Agent": "ACMOJ-Python-Client/2.2"
}

problem_id = 2298
git_url = "https://github.com/ojbench/oj-eval-openhands-071-20260423231558.git"

data = {"language": "git", "code": git_url}
url = f"{api_base}/problem/{problem_id}/submit"

print(f"Submitting to: {url}")
print(f"Git URL: {git_url}")

response = requests.post(url, headers=headers, data=data, timeout=30, proxies={"https": None, "http": None})

print(f"\nStatus Code: {response.status_code}")
print(f"Response: {response.text}")

if response.status_code in [200, 201]:
    result = response.json()
    print(json.dumps(result, indent=2))
    submission_id = result.get('id')
    print(f"\n✅ Submission successful! Submission ID: {submission_id}")
else:
    print("\n❌ Submission failed")
    exit(1)
