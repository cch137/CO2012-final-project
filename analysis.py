import json

with open("db.json") as f:
    data = json.load(f)

tag_dict = {
    key.split(":")[1]: value for key, value in data.items() if key.startswith("tag:")
}

user_dict = {
    key.split(":")[1]: {"name": name}
    for key, name in data.items()
    if key.startswith("user:") and key.endswith(":name")
}


def repalce_tag_id(tag: str):
    for t in tag_dict:
        if tag.startswith(t):
            return tag.replace(t, tag_dict[t])
    return tag


result_dict = {}

for user_id in sorted(user_dict.keys()):
    user_name = user_dict[user_id]["name"]
    user = {}
    user["atags"] = sorted(
        [repalce_tag_id(i) for i in data.get(f"user:{user_id}:atags", [])]
    )
    user["ptags"] = sorted(
        [repalce_tag_id(i) for i in data.get(f"user:{user_id}:ptags", [])]
    )
    result_dict[user_name] = user

stringified = json.dumps(result_dict, indent=4, sort_keys=True)

with open("analysis.json", "w") as f:
    f.write(stringified)
