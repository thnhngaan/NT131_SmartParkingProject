const { spawn } = require('child_process');
const path = require('path');

function runYOLO(imagePath, uid = '', token = '') {
  return new Promise((resolve, reject) => {
    const scriptPath = path.join(__dirname, '..', 'ai', 'detect.py');
    const pythonCommand = process.env.PYTHON_BIN || 'python';
    const child = spawn(
      pythonCommand,
      [scriptPath, imagePath, uid, token],
      {
        cwd: path.join(__dirname, '..'),
        env: process.env,
      }
    );

    let stdout = '';
    let stderr = '';

    child.stdout.on('data', (chunk) => {
      stdout += chunk.toString();
    });

    child.stderr.on('data', (chunk) => {
      stderr += chunk.toString();
    });

    child.on('error', (error) => {
      reject(error);
    });

    child.on('close', (code) => {
      if (code !== 0) {
        const error = new Error(`YOLO process exited with code ${code}`);
        error.stderr = stderr.trim();
        error.stdout = stdout.trim();
        return reject(error);
      }

      try {
        const output = stdout.toString();

        // 🔥 Tìm JSON trong output (bỏ hết log rác phía trước)
        const jsonStart = output.indexOf('{');
        const jsonEnd = output.lastIndexOf('}');

        if (jsonStart === -1 || jsonEnd === -1) {
          throw new Error("No valid JSON found in YOLO output");
        }

        const cleanJson = output.slice(jsonStart, jsonEnd + 1);

        const parsed = JSON.parse(cleanJson);
        resolve(parsed);

      } catch (error) {
        error.message = `Failed to parse YOLO response: ${error.message}`;
        error.stdout = stdout.trim();
        error.stderr = stderr.trim();
        reject(error);
      }
    });
  });
}

module.exports = runYOLO;
