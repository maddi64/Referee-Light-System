from flask import Flask, request, render_template
import os
import shutil
import zipfile

app = Flask(__name__)

# Constants
UPLOAD_FOLDER = os.path.join(os.getcwd(), 'owlcms')
TARGET_DIR = '/home/mwu/.local/share/owlcms/58.3.2'

app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/upload', methods=['POST'])
def upload():
    file = request.files.get('file')
    if not file:
        return "❌ No file uploaded", 400

    filename = file.filename.lower()
    if not filename.endswith('.zip'):
        return "Invalid file. Please upload a .zip file.", 400

    zip_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    file.save(zip_path)

    try:
        # Step 1: Delete all folders and files inside the target directory
        for item in os.listdir(TARGET_DIR):
            item_path = os.path.join(TARGET_DIR, item)
            if os.path.isdir(item_path):
                shutil.rmtree(item_path)
            else:
                os.remove(item_path)

        # Step 2: Extract uploaded zip into the target directory
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            zip_ref.extractall(TARGET_DIR)

        return "OWLCMS update successful. Files replaced."

    except Exception as e:
        return f"Error during update: {str(e)}", 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
