const fs = require('fs');
const path = require('path');

// Copy WASM files from build-wasm to extension out directory
const buildWasmDir = path.join(__dirname, '..', 'build-wasm');
const outDir = path.join(__dirname, 'out');

// Ensure out directory exists
if (!fs.existsSync(outDir)) {
    fs.mkdirSync(outDir, { recursive: true });
}

// Files to copy
const files = ['behl_lsp.wasm', 'behl_lsp.js'];

files.forEach(file => {
    const src = path.join(buildWasmDir, file);
    const dest = path.join(outDir, file);
    
    if (fs.existsSync(src)) {
        fs.copyFileSync(src, dest);
        console.log(`Copied ${file} to extension output`);
    } else {
        console.error(`Warning: ${src} not found. Run build-wasm.bat first!`);
    }
});

console.log('WASM files copy complete');
