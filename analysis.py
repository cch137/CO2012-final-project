import json

# Load JSON data from file
with open("db.json") as file:
    db_data = json.load(file)

# Extract tag mappings from data
tag_mapping = {
    key.split(":")[1]: value for key, value in db_data.items() if key.startswith("tag:")
}

# Extract user data from database
user_data = {
    key.split(":")[1]: {"name": name}
    for key, name in db_data.items()
    if key.startswith("user:") and key.endswith(":name")
}


# Replace tag ID with tag name
def replace_tag_id(tag: str):
    for tag_prefix in tag_mapping:
        if tag.startswith(tag_prefix):
            return tag.replace(tag_prefix, tag_mapping[tag_prefix])
    return tag


# Process user data with associated tags
processed_data = {}

for user_id in sorted(user_data.keys()):
    user_name = user_data[user_id]["name"]
    processed_data[user_name] = {
        "atags": sorted(
            [replace_tag_id(tag) for tag in db_data.get(f"user:{user_id}:atags", [])]
        ),
        "ptags": sorted(
            [replace_tag_id(tag) for tag in db_data.get(f"user:{user_id}:ptags", [])]
        ),
    }

# Write processed data to output file
with open("analysis.json", "w") as output_file:
    json.dump(processed_data, output_file, indent=4, sort_keys=True)


def calculate_tag_accuracy(file_path):
    # 讀取 JSON 檔案
    with open(file_path, "r") as f:
        data = json.load(f)

    total_difference = 0
    total_tags = 0

    for user, tags in data.items():
        if user == "popular":
            continue
        atags = {
            tag.split(":")[0]: float(tag.split(":")[1]) for tag in tags.get("atags", [])
        }
        ptags = {
            tag.split(":")[0]: float(tag.split(":")[1]) for tag in tags.get("ptags", [])
        }

        # 取得所有出現過的 tag
        all_tags = set(atags.keys()).union(ptags.keys())

        for tag in all_tags:
            atag_value = atags.get(tag, None)
            ptag_value = ptags.get(tag, None)

            if atag_value is not None or ptag_value is not None:
                atag_value = atag_value if atag_value is not None else 0
                ptag_value = ptag_value if ptag_value is not None else 0
                v = abs(ptag_value - atag_value)
                if atag_value == 0:
                    total_difference -= v
                else:
                    total_difference += v
                total_tags += 1

    # 計算平均差異
    accuracy = total_difference / total_tags if total_tags > 0 else 0
    return accuracy


accuracy = 1 - calculate_tag_accuracy("analysis.json")
print(f"ptags_accuracy={round(accuracy*100)}%")
