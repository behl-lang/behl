import * as vscode from 'vscode';
import * as path from 'path';
import { BehlLSPClient } from './lsp-client';

let client: BehlLSPClient | undefined;
let diagnosticCollection: vscode.DiagnosticCollection;

export async function activate(context: vscode.ExtensionContext) {
    console.log('behl language extension activating...');

    diagnosticCollection = vscode.languages.createDiagnosticCollection('behl');
    context.subscriptions.push(diagnosticCollection);

    try {
        // Initialize WASM LSP client
        const wasmPath = path.join(context.extensionPath, 'out', 'behl_lsp.js');
        const workspaceFolders = vscode.workspace.workspaceFolders;
        const workspaceRoot = workspaceFolders?.[0]?.uri.fsPath;

        console.log('[extension] Workspace root:', workspaceRoot);
        console.log('[extension] WASM path:', wasmPath);

        client = new BehlLSPClient(wasmPath, workspaceRoot);
        console.log('[extension] BehlLSPClient created, calling initialize...');
        await client.initialize();

        console.log('[extension] behl LSP initialized successfully');

        // Sync document changes to virtual filesystem
        context.subscriptions.push(
            vscode.workspace.onDidSaveTextDocument(document => {
                if (document.languageId === 'behl' && client) {
                    client.updateFile(document.uri.fsPath, document.getText());
                }
            })
        );

        // Sync document opens to virtual filesystem
        context.subscriptions.push(
            vscode.workspace.onDidOpenTextDocument(document => {
                if (document.languageId === 'behl') {
                    if (client) {
                        client.updateFile(document.uri.fsPath, document.getText());
                    }
                    updateDiagnostics(document);
                }
            })
        );

        // Register diagnostics on document changes
        context.subscriptions.push(
            vscode.workspace.onDidChangeTextDocument(event => {
                if (event.document.languageId === 'behl') {
                    updateDiagnostics(event.document);
                }
            })
        );

        // Register diagnostics on document open
        context.subscriptions.push(
            vscode.workspace.onDidOpenTextDocument(document => {
                if (document.languageId === 'behl') {
                    updateDiagnostics(document);
                }
            })
        );

        // Register completion provider
        context.subscriptions.push(
            vscode.languages.registerCompletionItemProvider(
                'behl',
                {
                    async provideCompletionItems(document, position) {
                        if (!client) return undefined;

                        const text = document.getText();
                        const filePath = document.uri.fsPath;
                        const completions = await client.getCompletions(
                            text,
                            position.line,
                            position.character,
                            filePath
                        );

                        return completions.map(item => {
                            const completion = new vscode.CompletionItem(
                                item.label,
                                vscode.CompletionItemKind.Function
                            );
                            completion.detail = item.detail;
                            return completion;
                        });
                    }
                },
                '.' // Trigger character for member access completions
            )
        );

        // Register hover provider
        context.subscriptions.push(
            vscode.languages.registerHoverProvider('behl', {
                async provideHover(document, position) {
                    if (!client) return undefined;

                    const text = document.getText();
                    const hoverInfo = await client.getHoverInfo(
                        text,
                        position.line,
                        position.character
                    );

                    if (hoverInfo) {
                        return new vscode.Hover(hoverInfo);
                    }
                    return undefined;
                }
            })
        );

        // Register signature help provider
        context.subscriptions.push(
            vscode.languages.registerSignatureHelpProvider(
                'behl',
                {
                    async provideSignatureHelp(document, position) {
                        if (!client) return undefined;

                        const text = document.getText();
                        const filePath = document.uri.fsPath;
                        const sigHelp = await client.getSignatureHelp(
                            text,
                            position.line,
                            position.character,
                            filePath
                        );

                        if (!sigHelp) return undefined;

                        const signature = new vscode.SignatureInformation(
                            `${sigHelp.functionName}(${sigHelp.parameters})`
                        );

                        // Split parameters and create parameter information
                        const params = sigHelp.parameters.split(',').map(p => p.trim()).filter(p => p);
                        signature.parameters = params.map(param => new vscode.ParameterInformation(param));

                        const result = new vscode.SignatureHelp();
                        result.signatures = [signature];
                        result.activeSignature = 0;
                        result.activeParameter = Math.min(sigHelp.activeParameter, params.length - 1);

                        return result;
                    }
                },
                '(', ',' // Trigger signature help on '(' and ','
            )
        );

        // Update diagnostics for all open behl documents
        vscode.workspace.textDocuments.forEach(document => {
            if (document.languageId === 'behl') {
                updateDiagnostics(document);
            }
        });

    } catch (error) {
        vscode.window.showErrorMessage(`Failed to initialize behl LSP: ${error}`);
        console.error('behl LSP initialization error:', error);
    }
}

async function updateDiagnostics(document: vscode.TextDocument) {
    if (!client) return;

    try {
        const text = document.getText();
        const filePath = document.uri.fsPath;
        const diagnostics = await client.getDiagnostics(text, filePath);

        const vscodeDiagnostics = diagnostics.map(diag => {
            // Parser returns 1-indexed lines, VSCode expects 0-indexed
            const range = new vscode.Range(
                diag.line - 1,
                diag.column,
                diag.line - 1,
                diag.column + diag.length
            );

            const severity =
                diag.severity === 'error'
                    ? vscode.DiagnosticSeverity.Error
                    : vscode.DiagnosticSeverity.Warning;

            return new vscode.Diagnostic(range, diag.message, severity);
        });

        diagnosticCollection.set(document.uri, vscodeDiagnostics);
    } catch (error) {
        console.error('Error updating diagnostics:', error);
    }
}

export function deactivate() {
    if (client) {
        client.dispose();
        client = undefined;
    }
    if (diagnosticCollection) {
        diagnosticCollection.dispose();
    }
}
