from flask import Flask, request, jsonify
import base64

app = Flask(__name__)

@app.route('/camera', methods=['POST'])
def upload_image():
    try:
        # Get image data from request body
        json_data = request.get_json(False,True,False)
        if json_data is None:
            return jsonify({'error': 'Failed to read request as json'}), 400
        
        image_base64=json_data['image'] # Read base64 image from json 
        print(f"Received image") #For Debug
        image_bytes=base64.b64decode(image_base64) # Decode from base64 to binary

        id=json_data['id']  # Read device id from json
        f = open(f"images/{id}.jpg", "wb")  # Save image in file with the id in the name
        f.write(image_bytes)
        f.close()
        print(f"Image saved correctly at \"images/{id}.jpg\"")
        return jsonify({'status': 'success'})

    except Exception as e:
        print("Error:", e)
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    # Use host='0.0.0.0' to make it accessible on your local network
    app.run(host='0.0.0.0', port=80)