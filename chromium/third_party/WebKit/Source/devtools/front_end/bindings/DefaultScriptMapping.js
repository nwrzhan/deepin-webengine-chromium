/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * @implements {Bindings.DebuggerSourceMapping}
 * @unrestricted
 */
Bindings.DefaultScriptMapping = class {
  /**
   * @param {!SDK.DebuggerModel} debuggerModel
   * @param {!Workspace.Workspace} workspace
   * @param {!Bindings.DebuggerWorkspaceBinding} debuggerWorkspaceBinding
   */
  constructor(debuggerModel, workspace, debuggerWorkspaceBinding) {
    this._debuggerModel = debuggerModel;
    this._debuggerWorkspaceBinding = debuggerWorkspaceBinding;
    var projectId = Bindings.DefaultScriptMapping.projectIdForTarget(debuggerModel.target());
    this._project = new Bindings.ContentProviderBasedProject(workspace, projectId, Workspace.projectTypes.Debugger, '');
    /** @type {!Map.<string, !Workspace.UISourceCode>} */
    this._uiSourceCodeForScriptId = new Map();
    /** @type {!Map.<!Workspace.UISourceCode, string>} */
    this._scriptIdForUISourceCode = new Map();
    this._eventListeners =
        [debuggerModel.addEventListener(SDK.DebuggerModel.Events.GlobalObjectCleared, this._debuggerReset, this)];
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode
   * @return {?SDK.Script}
   */
  static scriptForUISourceCode(uiSourceCode) {
    return uiSourceCode[Bindings.DefaultScriptMapping._scriptSymbol] || null;
  }

  /**
   * @param {!SDK.Target} target
   * @return {string}
   */
  static projectIdForTarget(target) {
    return 'debugger:' + target.id();
  }

  /**
   * @override
   * @param {!SDK.DebuggerModel.Location} rawLocation
   * @return {!Workspace.UILocation}
   */
  rawLocationToUILocation(rawLocation) {
    var debuggerModelLocation = /** @type {!SDK.DebuggerModel.Location} */ (rawLocation);
    var script = debuggerModelLocation.script();
    var uiSourceCode = this._uiSourceCodeForScriptId.get(script.scriptId);
    var lineNumber = debuggerModelLocation.lineNumber - (script.isInlineScriptWithSourceURL() ? script.lineOffset : 0);
    var columnNumber = debuggerModelLocation.columnNumber || 0;
    if (script.isInlineScriptWithSourceURL() && !lineNumber && columnNumber)
      columnNumber -= script.columnOffset;
    return uiSourceCode.uiLocation(lineNumber, columnNumber);
  }

  /**
   * @override
   * @param {!Workspace.UISourceCode} uiSourceCode
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {?SDK.DebuggerModel.Location}
   */
  uiLocationToRawLocation(uiSourceCode, lineNumber, columnNumber) {
    var scriptId = this._scriptIdForUISourceCode.get(uiSourceCode);
    var script = this._debuggerModel.scriptForId(scriptId);
    if (script.isInlineScriptWithSourceURL()) {
      return this._debuggerModel.createRawLocation(
          script, lineNumber + script.lineOffset, lineNumber ? columnNumber : columnNumber + script.columnOffset);
    }
    return this._debuggerModel.createRawLocation(script, lineNumber, columnNumber);
  }

  /**
   * @param {!SDK.Script} script
   */
  addScript(script) {
    var name = Common.ParsedURL.extractName(script.sourceURL);
    var url = 'debugger:///VM' + script.scriptId + (name ? ' ' + name : '');

    var uiSourceCode = this._project.createUISourceCode(url, Common.resourceTypes.Script);
    uiSourceCode[Bindings.DefaultScriptMapping._scriptSymbol] = script;
    this._uiSourceCodeForScriptId.set(script.scriptId, uiSourceCode);
    this._scriptIdForUISourceCode.set(uiSourceCode, script.scriptId);
    this._project.addUISourceCodeWithProvider(uiSourceCode, script, null);

    this._debuggerWorkspaceBinding.setSourceMapping(this._debuggerModel.target(), uiSourceCode, this);
    this._debuggerWorkspaceBinding.pushSourceMapping(script, this);
  }

  /**
   * @override
   * @return {boolean}
   */
  isIdentity() {
    return true;
  }

  /**
   * @override
   * @param {!Workspace.UISourceCode} uiSourceCode
   * @param {number} lineNumber
   * @return {boolean}
   */
  uiLineHasMapping(uiSourceCode, lineNumber) {
    return true;
  }

  _debuggerReset() {
    this._uiSourceCodeForScriptId.clear();
    this._scriptIdForUISourceCode.clear();
    this._project.reset();
  }

  dispose() {
    Common.EventTarget.removeEventListeners(this._eventListeners);
    this._debuggerReset();
    this._project.dispose();
  }
};

Bindings.DefaultScriptMapping._scriptSymbol = Symbol('symbol');
