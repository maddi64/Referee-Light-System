from flask import Flask, request, render_template, url_for
import os
import subprocess
import shutil

# Configuration
app = Flask(__name__)
BASE_DIR = os.getcwd()
UPLOAD_FOLDER = os.path.join(BASE_DIR, 'owlcms')
INSTALL_DIR = '/home/mwu/.local/share/owlcms'

app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/upload', methods=['POST'])
def upload():
    file = request.files.get('file')
    if not file:
        return "No file uploaded", 400

    filename = file.filename.lower()
    if not filename.endswith('.deb'):
        return "Invalid file. Please upload a .deb file containing 'owlcms' in the name.", 400

    save_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    file.save(save_path)

    try:
        # Step 1: Stop OWLCMS service
        stop = subprocess.run(['sudo', 'systemctl', 'stop', 'owlcms'], capture_output=True, text=True)
        if stop.returncode != 0:
            return f"Failed to stop OWLCMS:\nSTDOUT:\n{stop.stdout}\nSTDERR:\n{stop.stderr}", 500

        # Step 2: Delete existing OWLCMS folder
        if os.path.exists(INSTALL_DIR):
            shutil.rmtree(INSTALL_DIR)

        # Step 3: Install new OWLCMS .deb package
        install = subprocess.run(['sudo', 'dpkg', '-i', save_path], capture_output=True, text=True)
        if install.returncode != 0:
            return f"Failed to install package:\nSTDOUT:\n{install.stdout}\nSTDERR:\n{install.stderr}", 500

        # Step 4: Start OWLCMS service
        start = subprocess.run(['sudo', 'systemctl', 'start', 'owlcms'], capture_output=True, text=True)
        if start.returncode != 0:
            return f"Failed to start OWLCMS:\nSTDOUT:\n{start.stdout}\nSTDERR:\n{start.stderr}", 500

        return "OWLCMS was successfully updated and restarted."

    except Exception as e:
        return f"An unexpected Python error occurred:\n{str(e)}", 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
