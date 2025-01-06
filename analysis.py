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
