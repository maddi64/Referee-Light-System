from flask import Flask, request, render_template, url_for
import os
import subprocess
import shutil

app = Flask(__name__)
UPLOAD_FOLDER = os.path.join(os.getcwd(), 'owlcms')
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
    if not filename.endswith('.deb') or 'owlcms' not in filename:
        return "Invalid file. Please upload a .deb file containing 'owlcms' in the name.", 400

    save_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    file.save(save_path)

    try:
        # Step 1: Stop OWLCMS system service
        subprocess.run(['sudo', 'systemctl', 'stop', 'owlcms'], check=True)

        # Step 2: Delete current OWLCMS installation folder
        if os.path.exists(INSTALL_DIR):
            shutil.rmtree(INSTALL_DIR)

        # Step 3: Install the new .deb file
        install = subprocess.run(
            ['sudo', 'dpkg', '-i', save_path],
            capture_output=True,
            text=True
        )
        if install.returncode != 0:
            return f"Failed to install package:\n{install.stderr}", 500

        # Step 4: Restart OWLCMS service
        subprocess.run(['sudo', 'systemctl', 'start', 'owlcms'], check=True)

        return "OWLCMS successfully updated and restarted."

    except subprocess.CalledProcessError as e:
        return f"A system command failed:\n{e.stderr}", 500
    except Exception as e:
        return f"An unexpected error occurred: {str(e)}", 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
