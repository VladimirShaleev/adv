# Introduction {#introduction}

Adv is an embeddable scripting language with a compiler and a compact bytecode
virtual machine. Try editing this example in the browser:

@htmlonly
<section class="adv-playground" aria-label="Adv code editor">
  <div class="adv-playground-toolbar">
    <span class="adv-playground-title">Adv workspace</span>
    <div class="adv-playground-actions">
      <button class="adv-playground-button" id="adv-editor-copy" type="button">Copy file</button>
      <button class="adv-playground-button" id="adv-editor-reset" type="button">Reset</button>
      <button class="adv-playground-button" id="adv-editor-export" type="button">Export files</button>
    </div>
  </div>
  <div class="adv-workbench">
    <aside class="adv-file-explorer" aria-label="Workspace files">
      <div class="adv-file-explorer-heading">
        <span>FILES</span>
        <button class="adv-icon-button" id="adv-editor-new-file" type="button" aria-label="New file" title="New file">+</button>
      </div>
      <form class="adv-new-file-form" id="adv-new-file-form" hidden>
        <label class="adv-visually-hidden" for="adv-new-file-path">New file path</label>
        <input id="adv-new-file-path" type="text" placeholder="path/file.adv" autocomplete="off" spellcheck="false">
        <button class="adv-new-file-submit" type="submit">Add</button>
      </form>
      <ul class="adv-file-tree" id="adv-file-tree">
        <li><button class="adv-file-entry" type="button">main.adv</button></li>
        <li><button class="adv-file-entry" type="button">math.adv</button></li>
      </ul>
    </aside>
    <div class="adv-editor-pane">
      <div class="adv-editor-tabs" id="adv-editor-tabs" role="tablist" aria-label="Open files">
        <div class="adv-editor-tab is-active"><button class="adv-editor-tab-select" type="button" role="tab" aria-selected="true">main.adv</button></div>
      </div>
      <div id="adv-code-editor" class="adv-code-editor" aria-label="Adv source code editor"></div>
      <pre id="adv-editor-fallback" class="adv-playground-fallback"><code>import "math.adv";

var answer: int = fibonacci(8);
</code></pre>
    </div>
  </div>
  <dialog class="adv-confirm-dialog" id="adv-delete-file-dialog" aria-labelledby="adv-delete-file-message">
    <form class="adv-confirm-dialog-form">
      <p id="adv-delete-file-message"></p>
      <div class="adv-confirm-dialog-actions">
        <button class="adv-playground-button" type="submit" value="cancel">Cancel</button>
        <button class="adv-playground-button adv-delete-confirm" type="submit" value="delete">Delete</button>
      </div>
    </form>
  </dialog>
  <div class="adv-playground-footer">
    <span class="adv-editor-active-file" id="adv-editor-active-file">main.adv</span>
    <p id="adv-editor-status" class="adv-playground-status" role="status" aria-live="polite"></p>
  </div>
</section>
@endhtmlonly
