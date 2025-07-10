from flask import Flask, request, render_template, url_for
import os
import subprocess
import shutil

BASE_DIR = os.getcwd()
UPLOAD_FOLDER = os.path.join(BASE_DIR, 'owlcms')  # Save .deb here

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/upload', methods=['POST'])
def upload():
    file = request.files.get('file')
    if not file:
        return "No file uploaded", 400

    filename = file.filename.lower()
    if not filename.endswith('.deb') or 'owlcms' not in filename:
        return "Invalid file. Please upload a .deb file containing 'owlcms' in the name.", 400

    # Save uploaded file to owlcms folder
    save_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    file.save(save_path)

    try:
        # Step 1: Remove the existing OWLCMS package if installed
        uninstall = subprocess.run(
            ['sudo', 'dpkg', '-r', 'owlcms'],
            capture_output=True,
            text=True
        )
        print("Uninstall stdout:", uninstall.stdout)
        print("Uninstall stderr:", uninstall.stderr)

        # Step 2: Install the new OWLCMS package
        install = subprocess.run(
            ['sudo', 'dpkg', '-i', save_path],
            capture_output=True,
            text=True
        )
        if install.returncode != 0:
            return f"Failed to install package:\n{install.stderr}", 500

        # Step 3: Restart OWLCMS service (optional)
        subprocess.run(['sudo', 'systemctl', 'restart', 'owlcms'], check=False)

        return "OWLCMS was successfully updated and restarted!"

    except Exception as e:
        return f"An unexpected error occurred: {str(e)}", 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
