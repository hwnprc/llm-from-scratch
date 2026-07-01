import re
text = "Hello, world. This is a test"
result = re.split(r'([,.]|\s)', text)

# line for removing the blank
result = [item for item in result if item.strip()] 

print(result) 