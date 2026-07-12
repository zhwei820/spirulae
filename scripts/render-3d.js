"use strict";

function calcTransformMatrix(state, inverse = true,
        screenCenter = { x: 0.5, y: 0.5 }, aspect = null) {
    var sc = (state.height / Math.min(state.width, state.height)) / state.scale;
    if (aspect == null) aspect = canvas.width / canvas.height;
    var fov = (typeof state.fov == 'number' && isFinite(state.fov)) ?
            state.fov : 0.25 * Math.PI;
    sc *= Math.tan(0.125*Math.PI) / Math.tan(0.5*fov);
    var transformMatrix = mat4Perspective(
        fov, aspect,
        state.worldSpace ? 0.001*sc : 0.5*sc,
        state.worldSpace ? 0.01*sc : 10.0*sc);
    var translateMatrix = mat4Translate(mat4(1.0),
        [2.0*screenCenter.x-1.0, 2.0*screenCenter.y-1.0, 0.0]);
    if (typeof state.ry == 'number' && isFinite(state.ry)) {
        translateMatrix = mat4Scale(translateMatrix, [1.0/aspect, 1.0]);
        translateMatrix = mat4Rotate(translateMatrix, state.ry, [0, 0, 1]);
        translateMatrix = mat4Scale(translateMatrix, [aspect, 1.0]);
    }
    transformMatrix = mat4Mul(translateMatrix, transformMatrix);
    transformMatrix = mat4Translate(transformMatrix, [0, 0, -3.0 * sc]);
    transformMatrix = mat4Rotate(transformMatrix, state.rx, [1, 0, 0]);
    transformMatrix = mat4Rotate(transformMatrix, state.rz, [0, 0, 1]);
    if (!inverse) return transformMatrix;
    return mat4Inverse(transformMatrix);
}

function calcLightDirection(transformMatrix, lightTheta, lightPhi) {
    function dot(u, v) { return u[0] * v[0] + u[1] * v[1] + u[2] * v[2]; }
    // get uvw vectors
    var uvw = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
    for (var i = 0; i < 3; i++) {
        for (var j = 0; j < 3; j++) {
            uvw[i][j] = (transformMatrix[i][j] + transformMatrix[3][j]) / (transformMatrix[i][3] + transformMatrix[3][3]);
        }
    }
    var u = uvw[0], v = uvw[1], w = uvw[2];
    // orthogonalize and normalize the vectors
    var d = dot(w, w);
    for (var i = 0; i < 3; i++) w[i] /= Math.sqrt(d);
    for (var i = 0; i < 2; i++) {
        d = dot(uvw[i], w);
        for (var j = 0; j < 3; j++) uvw[i][j] -= w[j] * d;
        d = dot(uvw[i], uvw[i]);
        for (var j = 0; j < 3; j++) uvw[i][j] /= Math.sqrt(d);
        // note that u and v are not orthonogal due to translation of COM in the matrix
    }
    // calculate light direction
    var ku = Math.cos(lightTheta) * Math.sin(lightPhi);
    var kv = Math.sin(lightTheta) * Math.sin(lightPhi);
    var kw = -Math.cos(lightPhi);
    var l = [0, 0, 0];
    for (var i = 0; i < 3; i++)
        l[i] = ku * u[i] + kv * v[i] + kw * w[i];
    return l;
}

// labels attached to the positive ends of in-scene axes (e.g. Re/Im in complex3)
function renderAxesLabels(state, mat) {
    var labels = [
        document.getElementById("axis-x-label"),
        document.getElementById("axis-y-label")
    ];
    if (!labels[0] && !labels[1]) return;
    var nViews = (typeof renderer == 'object' && renderer.hzViews != null) ?
        renderer.hzViews.length : 1;
    var viewWidth = state.width / nViews;
    var showAxes = state.lAxes == 1 || state.lAxes == 2;
    var offset = {
        x: 2.0 * state.screenCenter.x - 1.0,
        y: 2.0 * state.screenCenter.y - 1.0
    };
    for (var i = 0; i < 2; i++) {
        if (!labels[i]) continue;
        // one copy of the label per view
        if (labels[i].viewClones == undefined)
            labels[i].viewClones = [];
        while (labels[i].viewClones.length < nViews - 1) {
            var clone = labels[i].cloneNode(true);
            clone.removeAttribute("id");
            document.body.appendChild(clone);
            labels[i].viewClones.push(clone);
        }
        var copies = [labels[i]].concat(labels[i].viewClones);
        var s = 1.02 * state.clipSize[i];
        var w = s * mat[i][3] + mat[3][3];
        if (!showAxes || !(w > 0.0)) {
            for (var v = 0; v < copies.length; v++)
                copies[v].style.display = "none";
            continue;
        }
        var x = (s * mat[i][0] + mat[3][0]) / w + offset.x;
        var y = (s * mat[i][1] + mat[3][1]) / w + offset.y;
        var px = (x + 1.0) * 0.5 * viewWidth;
        var py = (1.0 - y) * 0.5 * state.height;
        px = Math.max(16, Math.min(px, viewWidth - 16));
        py = Math.max(16, Math.min(py, state.height - 16));
        for (var v = 0; v < copies.length; v++) {
            copies[v].style.display = "block";
            copies[v].style.left = (v * viewWidth + px) + "px";
            copies[v].style.top = py + "px";
        }
    }
}

// set legend
function renderLegend(state) {
    var nViews = (typeof renderer == 'object' && renderer.hzViews != null) ?
        renderer.hzViews.length : 1;
    var viewWidth = state.width / nViews;
    // calculate axis length
    const targetLength = 36;
    var targetL = targetLength / (0.5 * Math.min(state.width, state.height) * state.scale);
    var expi = Math.floor(Math.log10(targetL));
    var l = Math.pow(10, expi);
    if (l / targetL < 0.2) l *= 5.0;
    if (l / targetL < 0.5) l *= 2.0;
    // get information
    let axes = [
        document.getElementById("axis-x"),
        document.getElementById("axis-y"),
        document.getElementById("axis-z")
    ];
    let yup_checkbox = document.getElementById("checkbox-yup");
    let yup = yup_checkbox && yup_checkbox.checked;
    let mat = calcTransformMatrix(state, false, { x: 0.5, y: 0.5 },
        nViews == 1 ? null : viewWidth / state.height);
    // set axes
    let ij = yup ? [0, 2, -1] : [0, 1, 2];
    for (var i = 0; i < 3; i++) {
        var j = Math.abs(ij[i]);
        var s = l * Math.sign(ij[i] + 1e-6);
        s *= Math.min(i == 2 ? calcZScale() : 1.0, 10.);
        var m = s * mat[j][3] + mat[3][3];
        var x = (s * mat[j][0] + mat[3][0]) / m * (0.5 * viewWidth);
        var y = (s * mat[j][1] + mat[3][1]) / m * (0.5 * state.height);
        var z = (s * mat[j][2] + mat[3][2]) / m;
        // if (!(z > 0. && z < 1.)) x = y = 0.;
        axes[i].setAttribute("x2", x);
        axes[i].setAttribute("y2", -y);
    }
    renderAxesLabels(state, mat);
    // set legend
    function toSuperscript(num) {
        num = "" + num;
        var res = "";
        for (var i = 0; i < num.length; i++) {
            if (num[i] == "-") res += "⁻";
            else res += "⁰¹²³⁴⁵⁶⁷⁸⁹"[Number(num[i])];
        }
        return res;
    }
    if (l >= 1e4 || l < 1e-3)
        l = Math.round(l * Math.pow(10, -expi)) + "×10" + toSuperscript(expi);
    document.getElementById("legend-text").textContent = l;
}
