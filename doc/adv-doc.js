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
          "import", "native", "function", "var", "as", "or", "and", "not",
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
      let activeFile;
      let pendingDelete;
      let editor;

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
        const encodedPath = file.split("/").map(encodeURIComponent).join("/");
        const uri = monaco.Uri.parse(`file:///adv-playground/${encodedPath}`);
        const model = monaco.editor.createModel(contents, "adv", uri);
        models.set(file, model);
      }

      function createFile(event) {
        event.preventDefault();
        const path = newFilePath.value.trim().replace(/\\/g, "/");
        if (!path || !/^[\w./-]+\.adv$/i.test(path) || path.split("/").some(part => !part || part === "." || part === "..")) {
          status.textContent = "Use a relative .adv path without empty, . or .. path segments.";
          newFilePath.focus();
          return;
        }
        if (models.has(path)) {
          openFile(path);
          status.textContent = `${path} is already in the workspace.`;
          newFileForm.hidden = true;
          return;
        }
        createModel(path, "");
        newFileForm.hidden = true;
        openFile(path);
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
        status.textContent = "Example workspace restored.";
        fallback.hidden = true;
      }

      function exportWorkspace() {
        const files = Object.fromEntries(
          [...models.entries()].sort(([first], [second]) => first.localeCompare(second))
            .map(([file, model]) => [file, model.getValue()])
        );
        const blob = new Blob([JSON.stringify(files, null, 2)], { type: "application/json" });
        const url = URL.createObjectURL(blob);
        const link = document.createElement("a");
        link.href = url;
        link.download = "adv-workspace.json";
        link.hidden = true;
        document.body.append(link);
        link.click();
        link.remove();
        window.setTimeout(() => URL.revokeObjectURL(url), 1000);
        status.textContent = "Workspace downloaded as adv-workspace.json.";
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
      newFilePath.addEventListener("keydown", event => {
        if (event.key === "Escape") {
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
      document.getElementById("adv-editor-export").addEventListener("click", exportWorkspace);

      document.getElementById("adv-editor-copy").addEventListener("click", async () => {
        try {
          await navigator.clipboard.writeText(editor.getValue());
          status.textContent = `${activeFile} copied to clipboard.`;
        } catch (error) {
          status.textContent = "Could not access the clipboard. Select and copy the code manually.";
        }
      });
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
