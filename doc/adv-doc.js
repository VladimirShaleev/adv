(() => {
  "use strict";

  const monacoVersion = "0.52.2";
  const monacoBaseUrl = `https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/${monacoVersion}/min`;
  const initialFiles = {
    "main.adv": `import "math.adv";

var answer: int = fibonacci(8);
`,
    "math.adv": `function fibonacci(n: int): int {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}
`
  };

  function initializeEditor() {
    const container = document.getElementById("adv-code-editor");
    if (!container) {
      return;
    }

    const status = document.getElementById("adv-editor-status");
    const fallback = document.getElementById("adv-editor-fallback");
    if (typeof window.require !== "function") {
      status.textContent = "The code editor could not be loaded. Check your connection and reload the page.";
      return;
    }

    window.MonacoEnvironment = {
      getWorkerUrl: () => {
        const workerSource = [
          `self.MonacoEnvironment = { baseUrl: '${monacoBaseUrl}/' };`,
          `importScripts('${monacoBaseUrl}/vs/base/worker/workerMain.js');`
        ].join("");
        return `data:text/javascript;charset=utf-8,${encodeURIComponent(workerSource)}`;
      }
    };

    window.require.config({ paths: { vs: `${monacoBaseUrl}/vs` } });
    window.require(["vs/editor/editor.main"], () => {
      const monaco = window.monaco;
      monaco.languages.register({ id: "adv" });
      monaco.languages.setLanguageConfiguration("adv", {
        comments: { lineComment: "//", blockComment: ["/*", "*/"] },
        brackets: [["{", "}"], ["[", "]"], ["(", ")"]],
        autoClosingPairs: [
          { open: "{", close: "}" },
          { open: "[", close: "]" },
          { open: "(", close: ")" },
          { open: "\"", close: "\"" }
        ],
        surroundingPairs: [
          { open: "{", close: "}" },
          { open: "[", close: "]" },
          { open: "(", close: ")" },
          { open: "\"", close: "\"" }
        ]
      });
      monaco.languages.setMonarchTokensProvider("adv", {
        keywords: [
          "import", "native", "function", "struct", "var", "as", "or", "and", "not",
          "if", "else", "while", "yield", "wait", "return"
        ],
        typeKeywords: ["bool", "int", "float", "string"],
        tokenizer: {
          root: [
            [/\/\/.*$/, "comment"],
            [/\/\*/, { token: "comment", next: "@comment" }],
            [/"([^"\\]|\\.)*$/, "string.invalid"],
            [/"/, { token: "string.quote", next: "@string" }],
            [/\d+\.\d+([eE][-+]?\d+)?/, "number.float"],
            [/\d+/, "number"],
            [/(:)(\s*)([a-zA-Z_]\w*)/, ["delimiter", "", "type"]],
            [/[a-zA-Z_]\w*/, {
              cases: {
                "@keywords": "keyword",
                "@typeKeywords": "type",
                "true": "constant",
                "false": "constant",
                "@default": "identifier"
              }
            }],
            [/<<<|>>>|<<|>>|==|!=|<=|>=/, "operator"],
            [/[=+\-*\/%<>~|&^]/, "operator"],
            [/[{}()[\]]/, "@brackets"],
            [/[,:;]/, "delimiter"]
          ],
          comment: [
            [/[^/*]+/, "comment"],
            [/\*\//, { token: "comment", next: "@pop" }],
            [/[/*]/, "comment"]
          ],
          string: [
            [/[^\\"]+/, "string"],
            [/\\./, "string.escape"],
            [/"/, { token: "string.quote", next: "@pop" }]
          ]
        }
      });

      const explorer = document.getElementById("adv-file-tree");
      const tabsContainer = document.getElementById("adv-editor-tabs");
      const newFileForm = document.getElementById("adv-new-file-form");
      const newFilePath = document.getElementById("adv-new-file-path");
      const deleteFileDialog = document.getElementById("adv-delete-file-dialog");
      const deleteFileMessage = document.getElementById("adv-delete-file-message");
      const models = new Map();
      const originalContents = new Map(Object.entries(initialFiles));
      const viewStates = new Map();
      const openFiles = [];
      const compileButton = document.getElementById("adv-editor-compile");
      const compilerOutput = document.getElementById("adv-compiler-output");
      const compilerMessages = document.getElementById("adv-compiler-messages");
      const compilerAssembly = document.getElementById("adv-compiler-assembly");
      const compilerPlaceholder = compilerAssembly.firstElementChild.cloneNode(true);
      const asmSections = new Set([".header", ".export", ".data", ".text"]);
      let activeFile;
      let pendingDelete;
      let editor;
      let compilerPromise;

      function formatCompilerMessage(message) {
        const statusCode = message.status.value;
        const severity = message.isError ? "Error" : statusCode === 1001 ? "Note" : "Warning";
        const location = message.filename
          ? `${message.filename}${message.line ? `:${message.line}` : ""}${message.column ? `:${message.column}` : ""}: `
          : "";
        return `${location}${severity}: ${message.message}`;
      }

      function appendAsmToken(parent, className, value) {
        if (!value) {
          return;
        }
        if (!className) {
          parent.append(document.createTextNode(value));
          return;
        }
        const token = document.createElement("span");
        token.className = className;
        token.textContent = value;
        parent.append(token);
      }

      function appendAsmTokens(line, text, section) {
        const pattern = /\s+|[A-Za-z_@$][A-Za-z0-9_.$@]*|-?\d+(?:\.\d+)*|"(?:\\.|[^"\\])*"|[^\s]/gy;
        let lastIndex = 0;
        let match;
        let expectingType = false;
        while ((match = pattern.exec(text))) {
          const value = match[0];
          if (/^\s+$/.test(value)) {
            appendAsmToken(line, "", value);
            lastIndex = pattern.lastIndex;
            continue;
          }
          appendAsmToken(line, "", text.slice(lastIndex, match.index));
          const next = text.slice(pattern.lastIndex).match(/^\s*:/);
          const isType = section === ".export" && expectingType;
          expectingType = value === ":";
          let className = "";
          if (value.startsWith("\"")) {
            className = "asm-string";
          } else if (/^-?\d/.test(value)) {
            className = "asm-number";
          } else if (/^[A-Za-z_@$]/.test(value)) {
            if (isType) {
              className = "asm-type";
            } else if (section === ".export" && value === "module") {
              className = "asm-keyword";
            } else if (section === ".export" && ["var", "native", "function", "struct"].includes(value)) {
              className = "asm-keyword";
            } else if (section === ".export" && ["bool", "int", "float", "string"].includes(value)) {
              className = "asm-type";
            } else if (section === ".header") {
              className = next ? "asm-property" : "asm-value";
            } else if (/^(depth|slots|params|rets)$/.test(value) && next) {
              className = "asm-property";
            } else if (/^(true|false)$/.test(value)) {
              className = "asm-constant";
            } else if (next) {
              className = "asm-property";
            } else {
              className = "asm-label";
            }
          } else if (/^[,:()]$/.test(value)) {
            className = "asm-punctuation";
          }
          appendAsmToken(line, className, value);
          lastIndex = pattern.lastIndex;
        }
        appendAsmToken(line, "", text.slice(lastIndex));
      }

      function highlightAssembly(code, text) {
        let section = "";
        const fragment = document.createDocumentFragment();
        text.split("\n").forEach((sourceLine, index, lines) => {
          const line = document.createElement("span");
          line.className = "asm-line";
          const trimmed = sourceLine.trim();
          if (asmSections.has(trimmed)) {
            section = trimmed;
            appendAsmToken(line, "asm-section", sourceLine);
          } else if (/^\s*\.function\b/.test(sourceLine)) {
            const match = sourceLine.match(/^(\s*)(\.function)(\s+)([A-Za-z_@$][A-Za-z0-9_.$@]*)(.*)$/);
            if (match) {
              appendAsmToken(line, "", match[1]);
              appendAsmToken(line, "asm-section", match[2]);
              appendAsmToken(line, "", match[3]);
              appendAsmToken(line, "asm-label", match[4]);
              appendAsmTokens(line, match[5], section);
            } else {
              appendAsmToken(line, "asm-section", sourceLine);
            }
          } else if ([".text", ".data"].includes(section) && /^\s*[A-Za-z_@$][A-Za-z0-9_.$@]*:/.test(sourceLine)) {
            const match = sourceLine.match(/^(\s*)([A-Za-z_@$][A-Za-z0-9_.$@]*)(:)(.*)$/);
            appendAsmToken(line, "", match[1]);
            appendAsmToken(line, "asm-label", match[2]);
            appendAsmToken(line, "asm-punctuation", match[3]);
            appendAsmTokens(line, match[4], section);
          } else if (section === ".export" && /^\s*(var|native|function|struct)\b/.test(sourceLine)) {
            const match = sourceLine.match(/^(\s*)(var|native|function|struct)(\s+)([A-Za-z_@$][A-Za-z0-9_.$@]*)(.*)$/);
            if (match) {
              appendAsmToken(line, "", match[1]);
              appendAsmToken(line, "asm-keyword", match[2]);
              appendAsmToken(line, "", match[3]);
              appendAsmToken(line, "asm-label", match[4]);
              appendAsmTokens(line, match[5], section);
            } else {
              appendAsmTokens(line, sourceLine, section);
            }
          } else if (/^\s*module\b/.test(sourceLine) && section === ".export") {
            appendAsmTokens(line, sourceLine, section);
          } else if (section === ".header" && /^\s*[A-Za-z_]\w*\s*:/.test(sourceLine)) {
            appendAsmTokens(line, sourceLine, section);
          } else if (/^\s+\S/.test(sourceLine)) {
            const match = sourceLine.match(/^(\s*)(\S+)(.*)$/);
            appendAsmToken(line, "", match[1]);
            appendAsmToken(line, "asm-opcode", match[2]);
            appendAsmTokens(line, match[3], section);
          } else {
            appendAsmToken(line, "", sourceLine);
          }
          fragment.append(line);
          if (index < lines.length - 1) {
            fragment.append(document.createTextNode("\n"));
          }
        });
        code.replaceChildren(fragment);
      }

      function showCompilerOutput(outputs) {
        compilerAssembly.replaceChildren();
        outputs.forEach(output => {
          const section = document.createElement("section");
          section.className = "adv-compiler-file";
          const heading = document.createElement("h3");
          heading.textContent = output.name;
          const code = document.createElement("pre");
          const highlighted = document.createElement("code");
          highlighted.className = "language-adv-asm";
          highlightAssembly(highlighted, output.text);
          code.append(highlighted);
          section.append(heading, code);
          compilerAssembly.append(section);
        });
      }

      function restoreCompilerPlaceholder() {
        compilerMessages.textContent = "";
        compilerAssembly.replaceChildren(compilerPlaceholder.cloneNode(true));
        compilerOutput.hidden = false;
      }

      async function compileActiveFile() {
        compilerAssembly.replaceChildren();
        compilerMessages.textContent = "";
        compilerOutput.hidden = true;
        if (!activeFile || !models.has(activeFile)) {
          status.textContent = "Open a file before compiling.";
          return;
        }

        compileButton.disabled = true;
        status.textContent = "Compiling...";
        let options;
        let result;
        try {
          if (!compilerPromise) {
            compilerPromise = import("./adv-js.esm.mjs")
              .then(({ default: initializeCompiler }) => initializeCompiler())
              .then(module => ({ module, compiler: new module.Compiler() }))
              .catch(error => {
                compilerPromise = undefined;
                throw error;
              });
          }
          const { module, compiler } = await compilerPromise;
          const sources = [
            { name: activeFile, data: models.get(activeFile).getValue() },
            ...[...models.entries()]
              .filter(([file]) => file !== activeFile)
              .map(([name, model]) => ({ name, data: model.getValue() }))
          ];
          const outputs = [];
          options = new module.Options();
          options.outputFormat = module.Format.TEXT;
          options.writer = source => {
            const text = typeof source.data === "string"
              ? source.data
              : new TextDecoder().decode(source.data);
            outputs.push({ name: source.name, text });
          };
          result = compiler.compile(null, sources, options);
          compilerMessages.textContent = result.messages.map(formatCompilerMessage).join("\n");
          if (result.hasErrors) {
            compilerOutput.hidden = false;
            status.textContent = "Compilation failed.";
            return;
          }
          showCompilerOutput(outputs);
          compilerOutput.hidden = false;
          const details = [];
          if (result.hasWarnings) {
            details.push("warnings");
          }
          if (result.hasNotes) {
            details.push("notes");
          }
          status.textContent = details.length
            ? `Compilation succeeded with ${details.join(" and ")}`
            : "Compilation succeeded";
        } catch (error) {
          compilerAssembly.replaceChildren();
          compilerMessages.textContent = error instanceof Error ? error.message : String(error);
          compilerOutput.hidden = false;
          status.textContent = "Compilation failed";
        } finally {
          if (result) {
            result.delete();
          }
          if (options) {
            options.delete();
          }
          compileButton.disabled = false;
        }
      }

      function isDirty(file) {
        return models.get(file).getValue() !== originalContents.get(file);
      }

      function refreshFileTree() {
        explorer.replaceChildren();
        [...models.keys()].sort().forEach(file => {
          const item = document.createElement("li");
          item.className = "adv-file-tree-item";
          const button = document.createElement("button");
          button.className = "adv-file-entry";
          button.type = "button";
          button.textContent = `${isDirty(file) ? "● " : ""}${file}`;
          button.title = file;
          button.setAttribute("aria-current", file === activeFile ? "page" : "false");
          button.addEventListener("click", () => openFile(file));
          const remove = document.createElement("button");
          remove.className = "adv-file-remove";
          remove.type = "button";
          remove.textContent = "×";
          remove.title = `Delete ${file}`;
          remove.setAttribute("aria-label", `Delete ${file}`);
          remove.addEventListener("click", () => requestDeleteFile(file));
          item.append(button, remove);
          explorer.append(item);
        });
      }

      function refreshTabs() {
        tabsContainer.replaceChildren();
        openFiles.forEach(file => {
          const tab = document.createElement("div");
          tab.className = `adv-editor-tab${file === activeFile ? " is-active" : ""}`;

          const select = document.createElement("button");
          select.className = "adv-editor-tab-select";
          select.type = "button";
          select.textContent = `${file}${isDirty(file) ? " •" : ""}`;
          select.title = file;
          select.setAttribute("role", "tab");
          select.setAttribute("aria-selected", file === activeFile ? "true" : "false");
          select.setAttribute("aria-current", file === activeFile ? "page" : "false");
          select.addEventListener("click", () => openFile(file));

          const close = document.createElement("button");
          close.className = "adv-editor-tab-close";
          close.type = "button";
          close.textContent = "×";
          close.title = `Close ${file}`;
          close.setAttribute("aria-label", `Close ${file}`);
          close.addEventListener("click", () => closeFile(file));

          tab.append(select, close);
          tabsContainer.append(tab);
        });
      }

      function openFile(file) {
        if (!models.has(file)) {
          return;
        }
        if (activeFile && activeFile !== file && editor.getModel()) {
          viewStates.set(activeFile, editor.saveViewState());
        }
        if (!openFiles.includes(file)) {
          openFiles.push(file);
        }
        activeFile = file;
        editor.setModel(models.get(file));
        if (viewStates.has(file)) {
          editor.restoreViewState(viewStates.get(file));
        }
        document.getElementById("adv-editor-active-file").textContent = file;
        status.textContent = "";
        refreshFileTree();
        refreshTabs();
      }

      function closeFile(file) {
        const index = openFiles.indexOf(file);
        if (index === -1) {
          return;
        }
        openFiles.splice(index, 1);
        if (activeFile === file) {
          if (editor.getModel()) {
            viewStates.set(file, editor.saveViewState());
          }
          const nextFile = openFiles[Math.min(index, openFiles.length - 1)];
          activeFile = undefined;
          if (nextFile) {
            openFile(nextFile);
          } else {
            editor.setModel(null);
            document.getElementById("adv-editor-active-file").textContent = "No file open";
          }
        }
        refreshTabs();
      }

      function deleteFile(file) {
        closeFile(file);
        if (activeFile === file) {
          activeFile = undefined;
          editor.setModel(null);
        }
        models.get(file).dispose();
        models.delete(file);
        originalContents.delete(file);
        viewStates.delete(file);
        refreshFileTree();
        status.textContent = `${file} deleted.`;
      }

      function requestDeleteFile(file) {
        pendingDelete = file;
        deleteFileMessage.textContent = `Delete ${file} from this workspace?`;
        deleteFileDialog.showModal();
      }

      function showNewFileForm() {
        newFileForm.hidden = false;
        newFilePath.value = "";
        newFilePath.focus();
      }

      function createModel(file, contents) {
        const uri = monaco.Uri.parse(`file:///adv-playground/${encodeURIComponent(file)}`);
        const model = monaco.editor.createModel(contents, "adv", uri);
        models.set(file, model);
      }

      function createFile(event) {
        event.preventDefault();
        const file = newFilePath.value;
        if (!/^[A-Za-z0-9_-][A-Za-z0-9_.-]*$/.test(file)) {
          status.textContent = "Use a file name containing only letters, digits, _, -, and . (for example, helper.adv).";
          newFilePath.focus();
          return;
        }
        if (models.has(file)) {
          openFile(file);
          status.textContent = `${file} is already in the workspace.`;
          newFileForm.hidden = true;
          return;
        }
        createModel(file, "");
        newFileForm.hidden = true;
        openFile(file);
      }

      function resetWorkspace() {
        if (editor) {
          editor.setModel(null);
        }
        models.forEach(model => model.dispose());
        models.clear();
        originalContents.clear();
        viewStates.clear();
        openFiles.length = 0;
        Object.entries(initialFiles).forEach(([file, contents]) => {
          originalContents.set(file, contents);
          createModel(file, contents);
        });
        activeFile = undefined;
        openFile("main.adv");
        restoreCompilerPlaceholder();
        status.textContent = "Example workspace restored.";
        fallback.hidden = true;
      }

      const isDarkMode = document.documentElement.classList.contains("dark-mode");
      monaco.editor.setTheme(isDarkMode ? "vs-dark" : "vs");
      const darkModeObserver = new MutationObserver(() => {
        monaco.editor.setTheme(document.documentElement.classList.contains("dark-mode") ? "vs-dark" : "vs");
      });
      darkModeObserver.observe(document.documentElement, { attributes: true, attributeFilter: ["class"] });

      Object.entries(initialFiles).forEach(([file, contents]) => createModel(file, contents));
      editor = monaco.editor.create(container, {
        automaticLayout: true,
        autoIndent: "full",
        bracketPairColorization: { enabled: true },
        cursorBlinking: "smooth",
        fontFamily: 'SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace',
        fontSize: 14,
        formatOnPaste: false,
        insertSpaces: true,
        lineNumbers: "on",
        minimap: { enabled: false },
        padding: { top: 12 },
        parameterHints: { enabled: false },
        quickSuggestions: false,
        scrollBeyondLastLine: false,
        suggestOnTriggerCharacters: false,
        tabSize: 4,
        theme: isDarkMode ? "vs-dark" : "vs",
        wordBasedSuggestions: "off",
        wordWrap: "off"
      });
      editor.onDidChangeModelContent(() => {
        refreshFileTree();
        refreshTabs();
      });
      fallback.hidden = true;
      openFile("main.adv");

      document.getElementById("adv-editor-new-file").addEventListener("click", showNewFileForm);
      newFileForm.addEventListener("submit", createFile);
      newFilePath.addEventListener("beforeinput", event => {
        if (event.data && /[\\/]/.test(event.data)) {
          event.preventDefault();
          status.textContent = "File names cannot contain '/' or '\\'.";
        }
      });
      newFilePath.addEventListener("paste", event => {
        if (/[\\/]/.test(event.clipboardData.getData("text"))) {
          event.preventDefault();
          status.textContent = "File names cannot contain '/' or '\\'.";
        }
      });
      newFilePath.addEventListener("keydown", event => {
        if (event.key === "/" || event.key === "\\") {
          event.preventDefault();
          status.textContent = "File names cannot contain '/' or '\\'.";
        } else if (event.key === "Escape") {
          newFileForm.hidden = true;
        }
      });
      deleteFileDialog.querySelector("form").addEventListener("submit", event => {
        event.preventDefault();
        const shouldDelete = event.submitter.value === "delete";
        deleteFileDialog.close();
        if (shouldDelete && pendingDelete) {
          deleteFile(pendingDelete);
        }
        pendingDelete = undefined;
      });
      deleteFileDialog.addEventListener("close", () => {
        pendingDelete = undefined;
      });
      document.getElementById("adv-editor-reset").addEventListener("click", resetWorkspace);
      compileButton.addEventListener("click", compileActiveFile);
    }, () => {
      status.textContent = "The code editor failed to load. Check your connection and reload the page.";
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initializeEditor, { once: true });
  } else {
    initializeEditor();
  }
})();
