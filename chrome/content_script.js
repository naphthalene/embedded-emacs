// Copyright (c) 2011 David Benjamin. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.

function positioned(elem) {
    var position = window.getComputedStyle(elem).position;
    return (position === "absolute" ||
            position === "relative" ||
            position === "fixed");
}

function hookTextArea(node, config) {
    function attachEmacs() {
        if (!node.parentNode)
            return;
        var container = document.createElement("div");
        var embed = document.createElement("embed");
        embed.type = "application/x-embedded-emacs-container";
        embed.style.setProperty("width", "100%", "important");
        embed.style.setProperty("height", "100%", "important");
        embed.style.setProperty("padding", "0px", "important");
        embed.style.setProperty("margin", "0px", "important");
        // WebKit refuses to focus an embed element unless it has a tabIndex.
        embed.setAttribute("tabIndex", "0");
        container.appendChild(embed);
        container.style.setProperty("border", "1px solid black", "important");
        container.style.setProperty("padding", "0px", "important");
        container.style.setProperty("margin", "0px", "important");
        function relayout() {
            container.style.setProperty("width", node.offsetWidth + "px",
                                        "important");
            container.style.setProperty("height", node.offsetHeight + "px",
                                        "important");
            // Apparently offsetParent is a lie.
            var parent = node.offsetParent;
            var top = node.offsetTop;
            var left = node.offsetLeft;
            var body = node.ownerDocument.body;
            var docElem = node.ownerDocument.documentElement;
            while (parent &&
                   parent !== body && parent !== docElem &&
                   !positioned(parent)) {
                top += parent.offsetTop;
                left += parent.offsetLeft;
                parent = parent.offsetParent;
            }
            container.style.setProperty("top", top + "px", "important");
            container.style.setProperty("left", left + "px", "important");
            if (window.getComputedStyle(node).position === "fixed") {
                container.style.setProperty("position", "fixed", "important");
            } else {
                container.style.setProperty("position", "absolute", "important");
            }
        }
        relayout();
        node.parentNode.insertBefore(container, node);

        // Make sure the plugin loaded.
        if (!embed.hasOwnProperty('windowId')) {
            console.log('Could not load container');
            container.parentNode.removeChild(container);
            return;
        }

        window.addEventListener("resize", relayout);

        // FIXME: If another script decides to change this value
        // again, act accordingly.
        var oldVisibility = node.style.visibility;
        node.style.visibility = "hidden";

        chrome.extension.sendRequest({
            type: "start_editor",
            text: node.value,
            windowId: embed.windowId
        }, function (text) {
            window.removeEventListener("resize", relayout);

            node.value = text;
            node.style.visibility = oldVisibility;
            if (container.parentNode)
                container.parentNode.removeChild(container);
        });

        // FIXME: This doesn't work.
        embed.focus();
    }

    if (config.triggerAltX)
        // Apparently I don't get keypress for Alt-X.
        node.addEventListener('keydown', function (evt) {
            if ((evt.altKey || evt.metaKey) &&
                !evt.ctrlKey &&
                !evt.shiftKey &&
                evt.keyCode === 88) {
                attachEmacs();
            }
        });
    if (config.triggerDoubleClick)
        node.addEventListener('dblclick', attachEmacs);
}

chrome.extension.sendRequest({
    type: 'get_config'
}, function (config) {
    if (config.enabled) {
        // Hook the current textareas.
        var textareas = document.getElementsByTagName("textarea");
        for (var i = 0; i < textareas.length; i++) {
            hookTextArea(textareas[i], config);
        }

        // And hook any new ones that get created.
        document.body.addEventListener('DOMNodeInserted', function (ev) {
            if (ev.target.nodeType != 1)
                        return;
            var textareas = ev.target.getElementsByTagName("textarea");
            for (var i = 0; i < textareas.length; i++) {
                hookTextArea(textareas[i], config);
            }
        });
    }
});
