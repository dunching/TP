import json
import time
import requests
import subprocess

slack_url = "https://hooks.slack.com/services/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"

commits_string = subprocess.run('git log SlackCommits..HEAD --pretty="format:%HCOMMIT_MID%BCOMMIT_END"', shell=True, check=True, capture_output=True, text=True).stdout

commits = commits_string.split("COMMIT_END")
commits = [commit.removeprefix("\n").removesuffix("\n") for commit in commits if len(commit) > 0]
commits = list(reversed(commits))

hashes_string = subprocess.run('git rev-list dev', shell=True, check=True, capture_output=True, text=True).stdout

hashes = hashes_string.split("\n")
hashes = list(reversed(hashes))

for hash_and_commit in commits:
    hash, commit = hash_and_commit.split("COMMIT_MID")
    commit = commit.removeprefix("\n").removesuffix("\n")
    
    if len(commit) == 0 or commit.startswith("Merge"):
        print("Ignoring " + commit)
        continue

    count = hashes.index(hash)

    commit = f"*{count}* {commit}"
    print(commit)
    
    data = {}
    data["text"] = commit

    result = requests.post(slack_url, data=json.dumps(data), headers={"Content-Type": "application/json"})
    result.raise_for_status()

    time.sleep(0.1)
    
subprocess.run('git tag -m SlackCommits -fa SlackCommits HEAD', shell=True, check=True)
subprocess.run('git push origin SlackCommits -f', shell=True, check=True)
