from flask import Flask, request, render_template
import os
import shutil
import zipfile
import subprocess
import sys

app = Flask(__name__)

# Constants
UPLOAD_FOLDER = '/tmp/owlcms_uploads'
TARGET_DIR = '/home/mwu/.local/share/owlcms/58.3.2'
UPDATER_DIR = os.path.dirname(os.path.abspath(__file__))

app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
app.config['MAX_CONTENT_LENGTH'] = 500 * 1024 * 1024
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/upload', methods=['POST'])
def upload():
    file = request.files.get('file')
    update_type = request.form.get('update_type')
    
    if not file:
        return "❌ No file uploaded", 400
    
    if not update_type:
        return "❌ Please select an update type", 400

    filename = file.filename.lower()
    if not filename.endswith('.zip'):
        return "❌ Invalid file. Please upload a .zip file.", 400

    zip_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    file.save(zip_path)

    try:
        if update_type == 'rlsx2':
            # RLSX2 System (OWLCMS) update
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

            return "✅ RLSX2 System (OWLCMS) update successful. Files replaced."
            
        elif update_type == 'firmware-updater':
            # Firmware Updater self-update
            temp_extract_dir = '/tmp/firmware_updater_temp'
            
            # Clean up any existing temp directory
            if os.path.exists(temp_extract_dir):
                shutil.rmtree(temp_extract_dir)
            
            # Extract the new firmware updater files to temp directory
            with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                zip_ref.extractall(temp_extract_dir)
            
            # Create update script that will replace files and restart
            update_script = f"""#!/bin/bash
sleep 2

# Stop the current process
pkill -f "python.*app.py"

# Backup current directory
cp -r "{UPDATER_DIR}" "{UPDATER_DIR}_backup_$(date +%Y%m%d_%H%M%S)"

# Remove old files (except backup)
find "{UPDATER_DIR}" -mindepth 1 -maxdepth 1 ! -name "*_backup_*" -exec rm -rf {{}} +

# Copy new files
cp -r "{temp_extract_dir}"/* "{UPDATER_DIR}/"

# Make app.py executable if needed
chmod +x "{UPDATER_DIR}/app.py"

# Clean up temp directory
rm -rf "{temp_extract_dir}"

# Restart the application
cd "{UPDATER_DIR}"
python3 app.py &

# Reboot the system to ensure clean restart
sudo reboot

# Clean up this script
rm "$0"
"""
            
            script_path = '/tmp/update_firmware_updater.sh'
            with open(script_path, 'w') as f:
                f.write(update_script)
            
            os.chmod(script_path, 0o755)
            
            # Execute the update script in background and return immediately
            subprocess.Popen(['/bin/bash', script_path], 
                           stdout=subprocess.DEVNULL, 
                           stderr=subprocess.DEVNULL)
            
            return "✅ Firmware Updater update initiated. System will restart automatically."
        
        else:
            return "❌ Invalid update type specified.", 400

    except Exception as e:
        return f"❌ Error during {update_type} update: {str(e)}", 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
