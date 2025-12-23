/**
 * Wrapper around the WASM-compiled Behl LSP module.
 * Provides a clean TypeScript interface to the C++ LSP implementation.
 */

import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';

export interface Diagnostic {
    line: number;
    column: number;
    length: number;
    message: string;
    severity: 'error' | 'warning';
}

export interface CompletionItem {
    label: string;
    detail?: string;
}

interface BehlLSPModule {
    LanguageServer: new () => {
        initialize(clientInfo: string): string;
        getDiagnostics(sourceCode: string, filePath: string): string;
        getCompletions(sourceCode: string, line: number, character: number, filePath: string): string;
        getHoverInfo(sourceCode: string, line: number, character: number): string;
        getSignatureHelp(sourceCode: string, line: number, character: number, filePath: string): string;
    };
    FS: any;
}

export class BehlLSPClient {
    private module: BehlLSPModule | null = null;
    private server: any | null = null;
    private wasmPath: string;
    private workspaceRoot: string;

    constructor(wasmPath: string, workspaceRoot?: string) {
        this.wasmPath = wasmPath;
        this.workspaceRoot = workspaceRoot || vscode.workspace.workspaceFolders?.[0]?.uri.fsPath || '';
    }

    async initialize(): Promise<void> {
        try {
            console.log('[initialize] Starting WASM module initialization...');
            console.log('[initialize] WASM path:', this.wasmPath);
            
            // Require the Emscripten-generated JS loader (CommonJS)
            const createModule = require(this.wasmPath);
            console.log('[initialize] WASM loader required successfully');
            
            // Initialize the WASM module
            this.module = await createModule();
            console.log('[initialize] WASM module created:', !!this.module);
            
            if (!this.module) {
                throw new Error('Failed to load WASM module');
            }

            console.log('[initialize] Module keys:', Object.keys(this.module).slice(0, 20).join(', '));
            console.log('[initialize] Module has FS:', 'FS' in this.module);
            console.log('[initialize] Module.FS type:', typeof this.module.FS);
            console.log('[initialize] Module.FS available:', !!this.module.FS);
            console.log('[initialize] Workspace root:', this.workspaceRoot);

            // Populate virtual filesystem with workspace .behl files
            if (this.workspaceRoot && this.module.FS) {
                console.log(`[initialize] Populating VFS from workspace: ${this.workspaceRoot}`);
                await this.populateVirtualFilesystem(this.workspaceRoot, '/');
                console.log(`[initialize] VFS population complete`);
            }

            console.log('[initialize] Creating LanguageServer instance...');
            // Create LSP server instance
            this.server = new this.module.LanguageServer();
            console.log('[initialize] LanguageServer created');
            
            // Initialize server
            const capabilities = this.server.initialize(JSON.stringify({
                clientName: 'vscode-behl',
                version: '0.1.0'
            }));

            console.log('[initialize] LSP Server capabilities:', capabilities);
        } catch (error) {
            console.error('[initialize] Failed to initialize WASM LSP:', error);
            throw error;
        }
    }

    private async populateVirtualFilesystem(hostDir: string, virtualDir: string): Promise<void> {
        if (!this.module?.FS) return;

        try {
            console.log(`[populateVFS] Scanning: ${hostDir} -> ${virtualDir}`);
            const entries = fs.readdirSync(hostDir, { withFileTypes: true });

            for (const entry of entries) {
                const hostPath = path.join(hostDir, entry.name);
                const virtualPath = path.posix.join(virtualDir, entry.name);

                if (entry.isDirectory()) {
                    // Create directory in virtual FS
                    console.log(`[populateVFS] Creating directory: ${virtualPath}`);
                    try {
                        this.module.FS.mkdir(virtualPath);
                    } catch (e) {
                        // Directory might already exist
                    }
                    // Recursively populate subdirectory
                    await this.populateVirtualFilesystem(hostPath, virtualPath);
                } else if (entry.isFile() && entry.name.endsWith('.behl')) {
                    // Read file content and write to virtual FS
                    const content = fs.readFileSync(hostPath, 'utf8');
                    this.module.FS.writeFile(virtualPath, content);
                    console.log(`[populateVFS] Loaded file: ${virtualPath}`);
                }
            }
        } catch (error) {
            console.warn(`Failed to populate virtual filesystem for ${hostDir}:`, error);
        }
    }

    updateFile(filePath: string, content: string): void {
        if (!this.module?.FS || !this.workspaceRoot) return;

        try {
            // Convert to workspace-relative path, then prepend / for VFS
            const relativePath = path.relative(this.workspaceRoot, filePath);
            const virtualPath = '/' + relativePath.replace(/\\/g, '/');
            
            // Ensure parent directories exist
            const dirPath = path.posix.dirname(virtualPath);
            if (dirPath && dirPath !== '/') {
                this.ensureDirectoryExists(dirPath);
            }
            
            // Update file in virtual FS
            this.module.FS.writeFile(virtualPath, content);
        } catch (error) {
            console.warn(`Failed to update file in virtual FS: ${filePath}`, error);
        }
    }

    private ensureDirectoryExists(dirPath: string): void {
        if (!this.module?.FS) return;

        const parts = dirPath.split('/').filter(p => p);
        let currentPath = '';
        
        for (const part of parts) {
            currentPath += '/' + part;
            try {
                this.module.FS.mkdir(currentPath);
            } catch (e) {
                // Directory might already exist
            }
        }
    }

    async getDiagnostics(sourceCode: string, filePath?: string): Promise<Diagnostic[]> {
        if (!this.server) {
            throw new Error('LSP server not initialized');
        }

        try {
            // Convert to workspace-relative path for diagnostics
            let relativePath = filePath || '<lsp>';
            if (filePath && this.workspaceRoot) {
                relativePath = path.relative(this.workspaceRoot, filePath);
                // Convert Windows backslashes to forward slashes
                relativePath = relativePath.replace(/\\/g, '/');
            }

            const result = this.server.getDiagnostics(sourceCode, relativePath);
            const diagnostics = JSON.parse(result);
            
            // Transform to our interface format
            return diagnostics.map((diag: any) => ({
                line: diag.line || 0,
                column: diag.column || 0,
                length: diag.length || 1,
                message: diag.message || 'Unknown error',
                severity: diag.severity || 'error'
            }));
        } catch (error) {
            console.error('Error getting diagnostics:', error);
            return [];
        }
    }

    async getCompletions(
        sourceCode: string,
        line: number,
        character: number,
        filePath?: string
    ): Promise<CompletionItem[]> {
        if (!this.server) {
            throw new Error('LSP server not initialized');
        }

        try {
            // Convert to workspace-relative path for import resolution
            let relativePath = filePath || '<lsp>';
            if (filePath && this.workspaceRoot) {
                console.log(`[getCompletions] workspaceRoot: ${this.workspaceRoot}`);
                console.log(`[getCompletions] filePath: ${filePath}`);
                relativePath = path.relative(this.workspaceRoot, filePath);
                console.log(`[getCompletions] relativePath (before slash fix): ${relativePath}`);
                // Convert Windows backslashes to forward slashes
                relativePath = relativePath.replace(/\\/g, '/');
                console.log(`[getCompletions] relativePath (final): ${relativePath}`);
            }

            const result = this.server.getCompletions(sourceCode, line, character, relativePath);
            const completions = JSON.parse(result);
            
            return completions.map((item: any) => ({
                label: item.label || '',
                detail: item.detail
            }));
        } catch (error) {
            console.error('Error getting completions:', error);
            return [];
        }
    }

    async getHoverInfo(
        sourceCode: string,
        line: number,
        character: number
    ): Promise<string | null> {
        if (!this.server) {
            throw new Error('LSP server not initialized');
        }

        try {
            const result = this.server.getHoverInfo(sourceCode, line, character);
            
            if (result === 'null' || !result) {
                return null;
            }

            const hoverData = JSON.parse(result);
            return hoverData.contents || null;
        } catch (error) {
            console.error('Error getting hover info:', error);
            return null;
        }
    }

    async getSignatureHelp(
        sourceCode: string,
        line: number,
        character: number,
        filePath?: string
    ): Promise<{ functionName: string; parameters: string; activeParameter: number } | null> {
        if (!this.server) {
            throw new Error('LSP server not initialized');
        }

        try {
            // Convert to workspace-relative path
            let relativePath = filePath || '<lsp>';
            if (filePath && this.workspaceRoot) {
                relativePath = path.relative(this.workspaceRoot, filePath);
                relativePath = relativePath.replace(/\\/g, '/');
            }

            const result = this.server.getSignatureHelp(sourceCode, line, character, relativePath);
            
            if (result === 'null' || !result) {
                return null;
            }

            return JSON.parse(result);
        } catch (error) {
            console.error('Error getting signature help:', error);
            return null;
        }
    }

    dispose(): void {
        this.server = null;
        this.module = null;
    }
}
