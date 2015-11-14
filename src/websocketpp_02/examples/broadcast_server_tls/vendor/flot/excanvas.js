// copyright 2006 google inc.
//
// licensed under the apache license, version 2.0 (the "license");
// you may not use this file except in compliance with the license.
// you may obtain a copy of the license at
//
//   http://www.apache.org/licenses/license-2.0
//
// unless required by applicable law or agreed to in writing, software
// distributed under the license is distributed on an "as is" basis,
// without warranties or conditions of any kind, either express or implied.
// see the license for the specific language governing permissions and
// limitations under the license.


// known issues:
//
// * patterns only support repeat.
// * radial gradient are not implemented. the vml version of these look very
//   different from the canvas one.
// * clipping paths are not implemented.
// * coordsize. the width and height attribute have higher priority than the
//   width and height style values which isn't correct.
// * painting mode isn't implemented.
// * canvas width/height should is using content-box by default. ie in
//   quirks mode will draw the canvas using border-box. either change your
//   doctype to html5
//   (http://www.whatwg.org/specs/web-apps/current-work/#the-doctype)
//   or use box sizing behavior from webfx
//   (http://webfx.eae.net/dhtml/boxsizing/boxsizing.html)
// * non uniform scaling does not correctly scale strokes.
// * filling very large shapes (above 5000 points) is buggy.
// * optimize. there is always room for speed improvements.

// only add this code if we do not already have a canvas implementation
if (!document.createelement('canvas').getcontext) {

(function() {

  // alias some functions to make (compiled) code shorter
  var m = math;
  var mr = m.round;
  var ms = m.sin;
  var mc = m.cos;
  var abs = m.abs;
  var sqrt = m.sqrt;

  // this is used for sub pixel precision
  var z = 10;
  var z2 = z / 2;

  /**
   * this funtion is assigned to the <canvas> elements as element.getcontext().
   * @this {htmlelement}
   * @return {canvasrenderingcontext2d_}
   */
  function getcontext() {
    return this.context_ ||
        (this.context_ = new canvasrenderingcontext2d_(this));
  }

  var slice = array.prototype.slice;

  /**
   * binds a function to an object. the returned function will always use the
   * passed in {@code obj} as {@code this}.
   *
   * example:
   *
   *   g = bind(f, obj, a, b)
   *   g(c, d) // will do f.call(obj, a, b, c, d)
   *
   * @param {function} f the function to bind the object to
   * @param {object} obj the object that should act as this when the function
   *     is called
   * @param {*} var_args rest arguments that will be used as the initial
   *     arguments when the function is called
   * @return {function} a new function that has bound this
   */
  function bind(f, obj, var_args) {
    var a = slice.call(arguments, 2);
    return function() {
      return f.apply(obj, a.concat(slice.call(arguments)));
    };
  }

  function encodehtmlattribute(s) {
    return string(s).replace(/&/g, '&amp;').replace(/"/g, '&quot;');
  }

  function addnamespacesandstylesheet(doc) {
    // create xmlns
    if (!doc.namespaces['g_vml_']) {
      doc.namespaces.add('g_vml_', 'urn:schemas-microsoft-com:vml',
                         '#default#vml');

    }
    if (!doc.namespaces['g_o_']) {
      doc.namespaces.add('g_o_', 'urn:schemas-microsoft-com:office:office',
                         '#default#vml');
    }

    // setup default css.  only add one style sheet per document
    if (!doc.stylesheets['ex_canvas_']) {
      var ss = doc.createstylesheet();
      ss.owningelement.id = 'ex_canvas_';
      ss.csstext = 'canvas{display:inline-block;overflow:hidden;' +
          // default size is 300x150 in gecko and opera
          'text-align:left;width:300px;height:150px}';
    }
  }

  // add namespaces and stylesheet at startup.
  addnamespacesandstylesheet(document);

  var g_vmlcanvasmanager_ = {
    init: function(opt_doc) {
      if (/msie/.test(navigator.useragent) && !window.opera) {
        var doc = opt_doc || document;
        // create a dummy element so that ie will allow canvas elements to be
        // recognized.
        doc.createelement('canvas');
        doc.attachevent('onreadystatechange', bind(this.init_, this, doc));
      }
    },

    init_: function(doc) {
      // find all canvas elements
      var els = doc.getelementsbytagname('canvas');
      for (var i = 0; i < els.length; i++) {
        this.initelement(els[i]);
      }
    },

    /**
     * public initializes a canvas element so that it can be used as canvas
     * element from now on. this is called automatically before the page is
     * loaded but if you are creating elements using createelement you need to
     * make sure this is called on the element.
     * @param {htmlelement} el the canvas element to initialize.
     * @return {htmlelement} the element that was created.
     */
    initelement: function(el) {
      if (!el.getcontext) {
        el.getcontext = getcontext;

        // add namespaces and stylesheet to document of the element.
        addnamespacesandstylesheet(el.ownerdocument);

        // remove fallback content. there is no way to hide text nodes so we
        // just remove all childnodes. we could hide all elements and remove
        // text nodes but who really cares about the fallback content.
        el.innerhtml = '';

        // do not use inline function because that will leak memory
        el.attachevent('onpropertychange', onpropertychange);
        el.attachevent('onresize', onresize);

        var attrs = el.attributes;
        if (attrs.width && attrs.width.specified) {
          // todo: use runtimestyle and coordsize
          // el.getcontext().setwidth_(attrs.width.nodevalue);
          el.style.width = attrs.width.nodevalue + 'px';
        } else {
          el.width = el.clientwidth;
        }
        if (attrs.height && attrs.height.specified) {
          // todo: use runtimestyle and coordsize
          // el.getcontext().setheight_(attrs.height.nodevalue);
          el.style.height = attrs.height.nodevalue + 'px';
        } else {
          el.height = el.clientheight;
        }
        //el.getcontext().setcoordsize_()
      }
      return el;
    }
  };

  function onpropertychange(e) {
    var el = e.srcelement;

    switch (e.propertyname) {
      case 'width':
        el.getcontext().clearrect();
        el.style.width = el.attributes.width.nodevalue + 'px';
        // in ie8 this does not trigger onresize.
        el.firstchild.style.width =  el.clientwidth + 'px';
        break;
      case 'height':
        el.getcontext().clearrect();
        el.style.height = el.attributes.height.nodevalue + 'px';
        el.firstchild.style.height = el.clientheight + 'px';
        break;
    }
  }

  function onresize(e) {
    var el = e.srcelement;
    if (el.firstchild) {
      el.firstchild.style.width =  el.clientwidth + 'px';
      el.firstchild.style.height = el.clientheight + 'px';
    }
  }

  g_vmlcanvasmanager_.init();

  // precompute "00" to "ff"
  var dectohex = [];
  for (var i = 0; i < 16; i++) {
    for (var j = 0; j < 16; j++) {
      dectohex[i * 16 + j] = i.tostring(16) + j.tostring(16);
    }
  }

  function creatematrixidentity() {
    return [
      [1, 0, 0],
      [0, 1, 0],
      [0, 0, 1]
    ];
  }

  function matrixmultiply(m1, m2) {
    var result = creatematrixidentity();

    for (var x = 0; x < 3; x++) {
      for (var y = 0; y < 3; y++) {
        var sum = 0;

        for (var z = 0; z < 3; z++) {
          sum += m1[x][z] * m2[z][y];
        }

        result[x][y] = sum;
      }
    }
    return result;
  }

  function copystate(o1, o2) {
    o2.fillstyle     = o1.fillstyle;
    o2.linecap       = o1.linecap;
    o2.linejoin      = o1.linejoin;
    o2.linewidth     = o1.linewidth;
    o2.miterlimit    = o1.miterlimit;
    o2.shadowblur    = o1.shadowblur;
    o2.shadowcolor   = o1.shadowcolor;
    o2.shadowoffsetx = o1.shadowoffsetx;
    o2.shadowoffsety = o1.shadowoffsety;
    o2.strokestyle   = o1.strokestyle;
    o2.globalalpha   = o1.globalalpha;
    o2.font          = o1.font;
    o2.textalign     = o1.textalign;
    o2.textbaseline  = o1.textbaseline;
    o2.arcscalex_    = o1.arcscalex_;
    o2.arcscaley_    = o1.arcscaley_;
    o2.linescale_    = o1.linescale_;
  }

  var colordata = {
    aliceblue: '#f0f8ff',
    antiquewhite: '#faebd7',
    aquamarine: '#7fffd4',
    azure: '#f0ffff',
    beige: '#f5f5dc',
    bisque: '#ffe4c4',
    black: '#000000',
    blanchedalmond: '#ffebcd',
    blueviolet: '#8a2be2',
    brown: '#a52a2a',
    burlywood: '#deb887',
    cadetblue: '#5f9ea0',
    chartreuse: '#7fff00',
    chocolate: '#d2691e',
    coral: '#ff7f50',
    cornflowerblue: '#6495ed',
    cornsilk: '#fff8dc',
    crimson: '#dc143c',
    cyan: '#00ffff',
    darkblue: '#00008b',
    darkcyan: '#008b8b',
    darkgoldenrod: '#b8860b',
    darkgray: '#a9a9a9',
    darkgreen: '#006400',
    darkgrey: '#a9a9a9',
    darkkhaki: '#bdb76b',
    darkmagenta: '#8b008b',
    darkolivegreen: '#556b2f',
    darkorange: '#ff8c00',
    darkorchid: '#9932cc',
    darkred: '#8b0000',
    darksalmon: '#e9967a',
    darkseagreen: '#8fbc8f',
    darkslateblue: '#483d8b',
    darkslategray: '#2f4f4f',
    darkslategrey: '#2f4f4f',
    darkturquoise: '#00ced1',
    darkviolet: '#9400d3',
    deeppink: '#ff1493',
    deepskyblue: '#00bfff',
    dimgray: '#696969',
    dimgrey: '#696969',
    dodgerblue: '#1e90ff',
    firebrick: '#b22222',
    floralwhite: '#fffaf0',
    forestgreen: '#228b22',
    gainsboro: '#dcdcdc',
    ghostwhite: '#f8f8ff',
    gold: '#ffd700',
    goldenrod: '#daa520',
    grey: '#808080',
    greenyellow: '#adff2f',
    honeydew: '#f0fff0',
    hotpink: '#ff69b4',
    indianred: '#cd5c5c',
    indigo: '#4b0082',
    ivory: '#fffff0',
    khaki: '#f0e68c',
    lavender: '#e6e6fa',
    lavenderblush: '#fff0f5',
    lawngreen: '#7cfc00',
    lemonchiffon: '#fffacd',
    lightblue: '#add8e6',
    lightcoral: '#f08080',
    lightcyan: '#e0ffff',
    lightgoldenrodyellow: '#fafad2',
    lightgreen: '#90ee90',
    lightgrey: '#d3d3d3',
    lightpink: '#ffb6c1',
    lightsalmon: '#ffa07a',
    lightseagreen: '#20b2aa',
    lightskyblue: '#87cefa',
    lightslategray: '#778899',
    lightslategrey: '#778899',
    lightsteelblue: '#b0c4de',
    lightyellow: '#ffffe0',
    limegreen: '#32cd32',
    linen: '#faf0e6',
    magenta: '#ff00ff',
    mediumaquamarine: '#66cdaa',
    mediumblue: '#0000cd',
    mediumorchid: '#ba55d3',
    mediumpurple: '#9370db',
    mediumseagreen: '#3cb371',
    mediumslateblue: '#7b68ee',
    mediumspringgreen: '#00fa9a',
    mediumturquoise: '#48d1cc',
    mediumvioletred: '#c71585',
    midnightblue: '#191970',
    mintcream: '#f5fffa',
    mistyrose: '#ffe4e1',
    moccasin: '#ffe4b5',
    navajowhite: '#ffdead',
    oldlace: '#fdf5e6',
    olivedrab: '#6b8e23',
    orange: '#ffa500',
    orangered: '#ff4500',
    orchid: '#da70d6',
    palegoldenrod: '#eee8aa',
    palegreen: '#98fb98',
    paleturquoise: '#afeeee',
    palevioletred: '#db7093',
    papayawhip: '#ffefd5',
    peachpuff: '#ffdab9',
    peru: '#cd853f',
    pink: '#ffc0cb',
    plum: '#dda0dd',
    powderblue: '#b0e0e6',
    rosybrown: '#bc8f8f',
    royalblue: '#4169e1',
    saddlebrown: '#8b4513',
    salmon: '#fa8072',
    sandybrown: '#f4a460',
    seagreen: '#2e8b57',
    seashell: '#fff5ee',
    sienna: '#a0522d',
    skyblue: '#87ceeb',
    slateblue: '#6a5acd',
    slategray: '#708090',
    slategrey: '#708090',
    snow: '#fffafa',
    springgreen: '#00ff7f',
    steelblue: '#4682b4',
    tan: '#d2b48c',
    thistle: '#d8bfd8',
    tomato: '#ff6347',
    turquoise: '#40e0d0',
    violet: '#ee82ee',
    wheat: '#f5deb3',
    whitesmoke: '#f5f5f5',
    yellowgreen: '#9acd32'
  };


  function getrgbhslcontent(stylestring) {
    var start = stylestring.indexof('(', 3);
    var end = stylestring.indexof(')', start + 1);
    var parts = stylestring.substring(start + 1, end).split(',');
    // add alpha if needed
    if (parts.length == 4 && stylestring.substr(3, 1) == 'a') {
      alpha = number(parts[3]);
    } else {
      parts[3] = 1;
    }
    return parts;
  }

  function percent(s) {
    return parsefloat(s) / 100;
  }

  function clamp(v, min, max) {
    return math.min(max, math.max(min, v));
  }

  function hsltorgb(parts){
    var r, g, b;
    h = parsefloat(parts[0]) / 360 % 360;
    if (h < 0)
      h++;
    s = clamp(percent(parts[1]), 0, 1);
    l = clamp(percent(parts[2]), 0, 1);
    if (s == 0) {
      r = g = b = l; // achromatic
    } else {
      var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
      var p = 2 * l - q;
      r = huetorgb(p, q, h + 1 / 3);
      g = huetorgb(p, q, h);
      b = huetorgb(p, q, h - 1 / 3);
    }

    return '#' + dectohex[math.floor(r * 255)] +
        dectohex[math.floor(g * 255)] +
        dectohex[math.floor(b * 255)];
  }

  function huetorgb(m1, m2, h) {
    if (h < 0)
      h++;
    if (h > 1)
      h--;

    if (6 * h < 1)
      return m1 + (m2 - m1) * 6 * h;
    else if (2 * h < 1)
      return m2;
    else if (3 * h < 2)
      return m1 + (m2 - m1) * (2 / 3 - h) * 6;
    else
      return m1;
  }

  function processstyle(stylestring) {
    var str, alpha = 1;

    stylestring = string(stylestring);
    if (stylestring.charat(0) == '#') {
      str = stylestring;
    } else if (/^rgb/.test(stylestring)) {
      var parts = getrgbhslcontent(stylestring);
      var str = '#', n;
      for (var i = 0; i < 3; i++) {
        if (parts[i].indexof('%') != -1) {
          n = math.floor(percent(parts[i]) * 255);
        } else {
          n = number(parts[i]);
        }
        str += dectohex[clamp(n, 0, 255)];
      }
      alpha = parts[3];
    } else if (/^hsl/.test(stylestring)) {
      var parts = getrgbhslcontent(stylestring);
      str = hsltorgb(parts);
      alpha = parts[3];
    } else {
      str = colordata[stylestring] || stylestring;
    }
    return {color: str, alpha: alpha};
  }

  var default_style = {
    style: 'normal',
    variant: 'normal',
    weight: 'normal',
    size: 10,
    family: 'sans-serif'
  };

  // internal text style cache
  var fontstylecache = {};

  function processfontstyle(stylestring) {
    if (fontstylecache[stylestring]) {
      return fontstylecache[stylestring];
    }

    var el = document.createelement('div');
    var style = el.style;
    try {
      style.font = stylestring;
    } catch (ex) {
      // ignore failures to set to invalid font.
    }

    return fontstylecache[stylestring] = {
      style: style.fontstyle || default_style.style,
      variant: style.fontvariant || default_style.variant,
      weight: style.fontweight || default_style.weight,
      size: style.fontsize || default_style.size,
      family: style.fontfamily || default_style.family
    };
  }

  function getcomputedstyle(style, element) {
    var computedstyle = {};

    for (var p in style) {
      computedstyle[p] = style[p];
    }

    // compute the size
    var canvasfontsize = parsefloat(element.currentstyle.fontsize),
        fontsize = parsefloat(style.size);

    if (typeof style.size == 'number') {
      computedstyle.size = style.size;
    } else if (style.size.indexof('px') != -1) {
      computedstyle.size = fontsize;
    } else if (style.size.indexof('em') != -1) {
      computedstyle.size = canvasfontsize * fontsize;
    } else if(style.size.indexof('%') != -1) {
      computedstyle.size = (canvasfontsize / 100) * fontsize;
    } else if (style.size.indexof('pt') != -1) {
      computedstyle.size = fontsize / .75;
    } else {
      computedstyle.size = canvasfontsize;
    }

    // different scaling between normal text and vml text. this was found using
    // trial and error to get the same size as non vml text.
    computedstyle.size *= 0.981;

    return computedstyle;
  }

  function buildstyle(style) {
    return style.style + ' ' + style.variant + ' ' + style.weight + ' ' +
        style.size + 'px ' + style.family;
  }

  function processlinecap(linecap) {
    switch (linecap) {
      case 'butt':
        return 'flat';
      case 'round':
        return 'round';
      case 'square':
      default:
        return 'square';
    }
  }

  /**
   * this class implements canvasrenderingcontext2d interface as described by
   * the whatwg.
   * @param {htmlelement} surfaceelement the element that the 2d context should
   * be associated with
   */
  function canvasrenderingcontext2d_(surfaceelement) {
    this.m_ = creatematrixidentity();

    this.mstack_ = [];
    this.astack_ = [];
    this.currentpath_ = [];

    // canvas context properties
    this.strokestyle = '#000';
    this.fillstyle = '#000';

    this.linewidth = 1;
    this.linejoin = 'miter';
    this.linecap = 'butt';
    this.miterlimit = z * 1;
    this.globalalpha = 1;
    this.font = '10px sans-serif';
    this.textalign = 'left';
    this.textbaseline = 'alphabetic';
    this.canvas = surfaceelement;

    var el = surfaceelement.ownerdocument.createelement('div');
    el.style.width =  surfaceelement.clientwidth + 'px';
    el.style.height = surfaceelement.clientheight + 'px';
    el.style.overflow = 'hidden';
    el.style.position = 'absolute';
    surfaceelement.appendchild(el);

    this.element_ = el;
    this.arcscalex_ = 1;
    this.arcscaley_ = 1;
    this.linescale_ = 1;
  }

  var contextprototype = canvasrenderingcontext2d_.prototype;
  contextprototype.clearrect = function() {
    if (this.textmeasureel_) {
      this.textmeasureel_.removenode(true);
      this.textmeasureel_ = null;
    }
    this.element_.innerhtml = '';
  };

  contextprototype.beginpath = function() {
    // todo: branch current matrix so that save/restore has no effect
    //       as per safari docs.
    this.currentpath_ = [];
  };

  contextprototype.moveto = function(ax, ay) {
    var p = this.getcoords_(ax, ay);
    this.currentpath_.push({type: 'moveto', x: p.x, y: p.y});
    this.currentx_ = p.x;
    this.currenty_ = p.y;
  };

  contextprototype.lineto = function(ax, ay) {
    var p = this.getcoords_(ax, ay);
    this.currentpath_.push({type: 'lineto', x: p.x, y: p.y});

    this.currentx_ = p.x;
    this.currenty_ = p.y;
  };

  contextprototype.beziercurveto = function(acp1x, acp1y,
                                            acp2x, acp2y,
                                            ax, ay) {
    var p = this.getcoords_(ax, ay);
    var cp1 = this.getcoords_(acp1x, acp1y);
    var cp2 = this.getcoords_(acp2x, acp2y);
    beziercurveto(this, cp1, cp2, p);
  };

  // helper function that takes the already fixed cordinates.
  function beziercurveto(self, cp1, cp2, p) {
    self.currentpath_.push({
      type: 'beziercurveto',
      cp1x: cp1.x,
      cp1y: cp1.y,
      cp2x: cp2.x,
      cp2y: cp2.y,
      x: p.x,
      y: p.y
    });
    self.currentx_ = p.x;
    self.currenty_ = p.y;
  }

  contextprototype.quadraticcurveto = function(acpx, acpy, ax, ay) {
    // the following is lifted almost directly from
    // http://developer.mozilla.org/en/docs/canvas_tutorial:drawing_shapes

    var cp = this.getcoords_(acpx, acpy);
    var p = this.getcoords_(ax, ay);

    var cp1 = {
      x: this.currentx_ + 2.0 / 3.0 * (cp.x - this.currentx_),
      y: this.currenty_ + 2.0 / 3.0 * (cp.y - this.currenty_)
    };
    var cp2 = {
      x: cp1.x + (p.x - this.currentx_) / 3.0,
      y: cp1.y + (p.y - this.currenty_) / 3.0
    };

    beziercurveto(this, cp1, cp2, p);
  };

  contextprototype.arc = function(ax, ay, aradius,
                                  astartangle, aendangle, aclockwise) {
    aradius *= z;
    var arctype = aclockwise ? 'at' : 'wa';

    var xstart = ax + mc(astartangle) * aradius - z2;
    var ystart = ay + ms(astartangle) * aradius - z2;

    var xend = ax + mc(aendangle) * aradius - z2;
    var yend = ay + ms(aendangle) * aradius - z2;

    // ie won't render arches drawn counter clockwise if xstart == xend.
    if (xstart == xend && !aclockwise) {
      xstart += 0.125; // offset xstart by 1/80 of a pixel. use something
                       // that can be represented in binary
    }

    var p = this.getcoords_(ax, ay);
    var pstart = this.getcoords_(xstart, ystart);
    var pend = this.getcoords_(xend, yend);

    this.currentpath_.push({type: arctype,
                           x: p.x,
                           y: p.y,
                           radius: aradius,
                           xstart: pstart.x,
                           ystart: pstart.y,
                           xend: pend.x,
                           yend: pend.y});

  };

  contextprototype.rect = function(ax, ay, awidth, aheight) {
    this.moveto(ax, ay);
    this.lineto(ax + awidth, ay);
    this.lineto(ax + awidth, ay + aheight);
    this.lineto(ax, ay + aheight);
    this.closepath();
  };

  contextprototype.strokerect = function(ax, ay, awidth, aheight) {
    var oldpath = this.currentpath_;
    this.beginpath();

    this.moveto(ax, ay);
    this.lineto(ax + awidth, ay);
    this.lineto(ax + awidth, ay + aheight);
    this.lineto(ax, ay + aheight);
    this.closepath();
    this.stroke();

    this.currentpath_ = oldpath;
  };

  contextprototype.fillrect = function(ax, ay, awidth, aheight) {
    var oldpath = this.currentpath_;
    this.beginpath();

    this.moveto(ax, ay);
    this.lineto(ax + awidth, ay);
    this.lineto(ax + awidth, ay + aheight);
    this.lineto(ax, ay + aheight);
    this.closepath();
    this.fill();

    this.currentpath_ = oldpath;
  };

  contextprototype.createlineargradient = function(ax0, ay0, ax1, ay1) {
    var gradient = new canvasgradient_('gradient');
    gradient.x0_ = ax0;
    gradient.y0_ = ay0;
    gradient.x1_ = ax1;
    gradient.y1_ = ay1;
    return gradient;
  };

  contextprototype.createradialgradient = function(ax0, ay0, ar0,
                                                   ax1, ay1, ar1) {
    var gradient = new canvasgradient_('gradientradial');
    gradient.x0_ = ax0;
    gradient.y0_ = ay0;
    gradient.r0_ = ar0;
    gradient.x1_ = ax1;
    gradient.y1_ = ay1;
    gradient.r1_ = ar1;
    return gradient;
  };

  contextprototype.drawimage = function(image, var_args) {
    var dx, dy, dw, dh, sx, sy, sw, sh;

    // to find the original width we overide the width and height
    var oldruntimewidth = image.runtimestyle.width;
    var oldruntimeheight = image.runtimestyle.height;
    image.runtimestyle.width = 'auto';
    image.runtimestyle.height = 'auto';

    // get the original size
    var w = image.width;
    var h = image.height;

    // and remove overides
    image.runtimestyle.width = oldruntimewidth;
    image.runtimestyle.height = oldruntimeheight;

    if (arguments.length == 3) {
      dx = arguments[1];
      dy = arguments[2];
      sx = sy = 0;
      sw = dw = w;
      sh = dh = h;
    } else if (arguments.length == 5) {
      dx = arguments[1];
      dy = arguments[2];
      dw = arguments[3];
      dh = arguments[4];
      sx = sy = 0;
      sw = w;
      sh = h;
    } else if (arguments.length == 9) {
      sx = arguments[1];
      sy = arguments[2];
      sw = arguments[3];
      sh = arguments[4];
      dx = arguments[5];
      dy = arguments[6];
      dw = arguments[7];
      dh = arguments[8];
    } else {
      throw error('invalid number of arguments');
    }

    var d = this.getcoords_(dx, dy);

    var w2 = sw / 2;
    var h2 = sh / 2;

    var vmlstr = [];

    var w = 10;
    var h = 10;

    // for some reason that i've now forgotten, using divs didn't work
    vmlstr.push(' <g_vml_:group',
                ' coordsize="', z * w, ',', z * h, '"',
                ' coordorigin="0,0"' ,
                ' style="width:', w, 'px;height:', h, 'px;position:absolute;');

    // if filters are necessary (rotation exists), create them
    // filters are bog-slow, so only create them if abbsolutely necessary
    // the following check doesn't account for skews (which don't exist
    // in the canvas spec (yet) anyway.

    if (this.m_[0][0] != 1 || this.m_[0][1] ||
        this.m_[1][1] != 1 || this.m_[1][0]) {
      var filter = [];

      // note the 12/21 reversal
      filter.push('m11=', this.m_[0][0], ',',
                  'm12=', this.m_[1][0], ',',
                  'm21=', this.m_[0][1], ',',
                  'm22=', this.m_[1][1], ',',
                  'dx=', mr(d.x / z), ',',
                  'dy=', mr(d.y / z), '');

      // bounding box calculation (need to minimize displayed area so that
      // filters don't waste time on unused pixels.
      var max = d;
      var c2 = this.getcoords_(dx + dw, dy);
      var c3 = this.getcoords_(dx, dy + dh);
      var c4 = this.getcoords_(dx + dw, dy + dh);

      max.x = m.max(max.x, c2.x, c3.x, c4.x);
      max.y = m.max(max.y, c2.y, c3.y, c4.y);

      vmlstr.push('padding:0 ', mr(max.x / z), 'px ', mr(max.y / z),
                  'px 0;filter:progid:dximagetransform.microsoft.matrix(',
                  filter.join(''), ", sizingmethod='clip');");

    } else {
      vmlstr.push('top:', mr(d.y / z), 'px;left:', mr(d.x / z), 'px;');
    }

    vmlstr.push(' ">' ,
                '<g_vml_:image src="', image.src, '"',
                ' style="width:', z * dw, 'px;',
                ' height:', z * dh, 'px"',
                ' cropleft="', sx / w, '"',
                ' croptop="', sy / h, '"',
                ' cropright="', (w - sx - sw) / w, '"',
                ' cropbottom="', (h - sy - sh) / h, '"',
                ' />',
                '</g_vml_:group>');

    this.element_.insertadjacenthtml('beforeend', vmlstr.join(''));
  };

  contextprototype.stroke = function(afill) {
    var w = 10;
    var h = 10;
    // divide the shape into chunks if it's too long because ie has a limit
    // somewhere for how long a vml shape can be. this simple division does
    // not work with fills, only strokes, unfortunately.
    var chunksize = 5000;

    var min = {x: null, y: null};
    var max = {x: null, y: null};

    for (var j = 0; j < this.currentpath_.length; j += chunksize) {
      var linestr = [];
      var lineopen = false;

      linestr.push('<g_vml_:shape',
                   ' filled="', !!afill, '"',
                   ' style="position:absolute;width:', w, 'px;height:', h, 'px;"',
                   ' coordorigin="0,0"',
                   ' coordsize="', z * w, ',', z * h, '"',
                   ' stroked="', !afill, '"',
                   ' path="');

      var newseq = false;

      for (var i = j; i < math.min(j + chunksize, this.currentpath_.length); i++) {
        if (i % chunksize == 0 && i > 0) { // move into position for next chunk
          linestr.push(' m ', mr(this.currentpath_[i-1].x), ',', mr(this.currentpath_[i-1].y));
        }

        var p = this.currentpath_[i];
        var c;

        switch (p.type) {
          case 'moveto':
            c = p;
            linestr.push(' m ', mr(p.x), ',', mr(p.y));
            break;
          case 'lineto':
            linestr.push(' l ', mr(p.x), ',', mr(p.y));
            break;
          case 'close':
            linestr.push(' x ');
            p = null;
            break;
          case 'beziercurveto':
            linestr.push(' c ',
                         mr(p.cp1x), ',', mr(p.cp1y), ',',
                         mr(p.cp2x), ',', mr(p.cp2y), ',',
                         mr(p.x), ',', mr(p.y));
            break;
          case 'at':
          case 'wa':
            linestr.push(' ', p.type, ' ',
                         mr(p.x - this.arcscalex_ * p.radius), ',',
                         mr(p.y - this.arcscaley_ * p.radius), ' ',
                         mr(p.x + this.arcscalex_ * p.radius), ',',
                         mr(p.y + this.arcscaley_ * p.radius), ' ',
                         mr(p.xstart), ',', mr(p.ystart), ' ',
                         mr(p.xend), ',', mr(p.yend));
            break;
        }
  
  
        // todo: following is broken for curves due to
        //       move to proper paths.
  
        // figure out dimensions so we can do gradient fills
        // properly
        if (p) {
          if (min.x == null || p.x < min.x) {
            min.x = p.x;
          }
          if (max.x == null || p.x > max.x) {
            max.x = p.x;
          }
          if (min.y == null || p.y < min.y) {
            min.y = p.y;
          }
          if (max.y == null || p.y > max.y) {
            max.y = p.y;
          }
        }
      }
      linestr.push(' ">');
  
      if (!afill) {
        appendstroke(this, linestr);
      } else {
        appendfill(this, linestr, min, max);
      }
  
      linestr.push('</g_vml_:shape>');
  
      this.element_.insertadjacenthtml('beforeend', linestr.join(''));
    }
  };

  function appendstroke(ctx, linestr) {
    var a = processstyle(ctx.strokestyle);
    var color = a.color;
    var opacity = a.alpha * ctx.globalalpha;
    var linewidth = ctx.linescale_ * ctx.linewidth;

    // vml cannot correctly render a line if the width is less than 1px.
    // in that case, we dilute the color to make the line look thinner.
    if (linewidth < 1) {
      opacity *= linewidth;
    }

    linestr.push(
      '<g_vml_:stroke',
      ' opacity="', opacity, '"',
      ' joinstyle="', ctx.linejoin, '"',
      ' miterlimit="', ctx.miterlimit, '"',
      ' endcap="', processlinecap(ctx.linecap), '"',
      ' weight="', linewidth, 'px"',
      ' color="', color, '" />'
    );
  }

  function appendfill(ctx, linestr, min, max) {
    var fillstyle = ctx.fillstyle;
    var arcscalex = ctx.arcscalex_;
    var arcscaley = ctx.arcscaley_;
    var width = max.x - min.x;
    var height = max.y - min.y;
    if (fillstyle instanceof canvasgradient_) {
      // todo: gradients transformed with the transformation matrix.
      var angle = 0;
      var focus = {x: 0, y: 0};

      // additional offset
      var shift = 0;
      // scale factor for offset
      var expansion = 1;

      if (fillstyle.type_ == 'gradient') {
        var x0 = fillstyle.x0_ / arcscalex;
        var y0 = fillstyle.y0_ / arcscaley;
        var x1 = fillstyle.x1_ / arcscalex;
        var y1 = fillstyle.y1_ / arcscaley;
        var p0 = ctx.getcoords_(x0, y0);
        var p1 = ctx.getcoords_(x1, y1);
        var dx = p1.x - p0.x;
        var dy = p1.y - p0.y;
        angle = math.atan2(dx, dy) * 180 / math.pi;

        // the angle should be a non-negative number.
        if (angle < 0) {
          angle += 360;
        }

        // very small angles produce an unexpected result because they are
        // converted to a scientific notation string.
        if (angle < 1e-6) {
          angle = 0;
        }
      } else {
        var p0 = ctx.getcoords_(fillstyle.x0_, fillstyle.y0_);
        focus = {
          x: (p0.x - min.x) / width,
          y: (p0.y - min.y) / height
        };

        width  /= arcscalex * z;
        height /= arcscaley * z;
        var dimension = m.max(width, height);
        shift = 2 * fillstyle.r0_ / dimension;
        expansion = 2 * fillstyle.r1_ / dimension - shift;
      }

      // we need to sort the color stops in ascending order by offset,
      // otherwise ie won't interpret it correctly.
      var stops = fillstyle.colors_;
      stops.sort(function(cs1, cs2) {
        return cs1.offset - cs2.offset;
      });

      var length = stops.length;
      var color1 = stops[0].color;
      var color2 = stops[length - 1].color;
      var opacity1 = stops[0].alpha * ctx.globalalpha;
      var opacity2 = stops[length - 1].alpha * ctx.globalalpha;

      var colors = [];
      for (var i = 0; i < length; i++) {
        var stop = stops[i];
        colors.push(stop.offset * expansion + shift + ' ' + stop.color);
      }

      // when colors attribute is used, the meanings of opacity and o:opacity2
      // are reversed.
      linestr.push('<g_vml_:fill type="', fillstyle.type_, '"',
                   ' method="none" focus="100%"',
                   ' color="', color1, '"',
                   ' color2="', color2, '"',
                   ' colors="', colors.join(','), '"',
                   ' opacity="', opacity2, '"',
                   ' g_o_:opacity2="', opacity1, '"',
                   ' angle="', angle, '"',
                   ' focusposition="', focus.x, ',', focus.y, '" />');
    } else if (fillstyle instanceof canvaspattern_) {
      if (width && height) {
        var deltaleft = -min.x;
        var deltatop = -min.y;
        linestr.push('<g_vml_:fill',
                     ' position="',
                     deltaleft / width * arcscalex * arcscalex, ',',
                     deltatop / height * arcscaley * arcscaley, '"',
                     ' type="tile"',
                     // todo: figure out the correct size to fit the scale.
                     //' size="', w, 'px ', h, 'px"',
                     ' src="', fillstyle.src_, '" />');
       }
    } else {
      var a = processstyle(ctx.fillstyle);
      var color = a.color;
      var opacity = a.alpha * ctx.globalalpha;
      linestr.push('<g_vml_:fill color="', color, '" opacity="', opacity,
                   '" />');
    }
  }

  contextprototype.fill = function() {
    this.stroke(true);
  };

  contextprototype.closepath = function() {
    this.currentpath_.push({type: 'close'});
  };

  /**
   * @private
   */
  contextprototype.getcoords_ = function(ax, ay) {
    var m = this.m_;
    return {
      x: z * (ax * m[0][0] + ay * m[1][0] + m[2][0]) - z2,
      y: z * (ax * m[0][1] + ay * m[1][1] + m[2][1]) - z2
    };
  };

  contextprototype.save = function() {
    var o = {};
    copystate(this, o);
    this.astack_.push(o);
    this.mstack_.push(this.m_);
    this.m_ = matrixmultiply(creatematrixidentity(), this.m_);
  };

  contextprototype.restore = function() {
    if (this.astack_.length) {
      copystate(this.astack_.pop(), this);
      this.m_ = this.mstack_.pop();
    }
  };

  function matrixisfinite(m) {
    return isfinite(m[0][0]) && isfinite(m[0][1]) &&
        isfinite(m[1][0]) && isfinite(m[1][1]) &&
        isfinite(m[2][0]) && isfinite(m[2][1]);
  }

  function setm(ctx, m, updatelinescale) {
    if (!matrixisfinite(m)) {
      return;
    }
    ctx.m_ = m;

    if (updatelinescale) {
      // get the line scale.
      // determinant of this.m_ means how much the area is enlarged by the
      // transformation. so its square root can be used as a scale factor
      // for width.
      var det = m[0][0] * m[1][1] - m[0][1] * m[1][0];
      ctx.linescale_ = sqrt(abs(det));
    }
  }

  contextprototype.translate = function(ax, ay) {
    var m1 = [
      [1,  0,  0],
      [0,  1,  0],
      [ax, ay, 1]
    ];

    setm(this, matrixmultiply(m1, this.m_), false);
  };

  contextprototype.rotate = function(arot) {
    var c = mc(arot);
    var s = ms(arot);

    var m1 = [
      [c,  s, 0],
      [-s, c, 0],
      [0,  0, 1]
    ];

    setm(this, matrixmultiply(m1, this.m_), false);
  };

  contextprototype.scale = function(ax, ay) {
    this.arcscalex_ *= ax;
    this.arcscaley_ *= ay;
    var m1 = [
      [ax, 0,  0],
      [0,  ay, 0],
      [0,  0,  1]
    ];

    setm(this, matrixmultiply(m1, this.m_), true);
  };

  contextprototype.transform = function(m11, m12, m21, m22, dx, dy) {
    var m1 = [
      [m11, m12, 0],
      [m21, m22, 0],
      [dx,  dy,  1]
    ];

    setm(this, matrixmultiply(m1, this.m_), true);
  };

  contextprototype.settransform = function(m11, m12, m21, m22, dx, dy) {
    var m = [
      [m11, m12, 0],
      [m21, m22, 0],
      [dx,  dy,  1]
    ];

    setm(this, m, true);
  };

  /**
   * the text drawing function.
   * the maxwidth argument isn't taken in account, since no browser supports
   * it yet.
   */
  contextprototype.drawtext_ = function(text, x, y, maxwidth, stroke) {
    var m = this.m_,
        delta = 1000,
        left = 0,
        right = delta,
        offset = {x: 0, y: 0},
        linestr = [];

    var fontstyle = getcomputedstyle(processfontstyle(this.font),
                                     this.element_);

    var fontstylestring = buildstyle(fontstyle);

    var elementstyle = this.element_.currentstyle;
    var textalign = this.textalign.tolowercase();
    switch (textalign) {
      case 'left':
      case 'center':
      case 'right':
        break;
      case 'end':
        textalign = elementstyle.direction == 'ltr' ? 'right' : 'left';
        break;
      case 'start':
        textalign = elementstyle.direction == 'rtl' ? 'right' : 'left';
        break;
      default:
        textalign = 'left';
    }

    // 1.75 is an arbitrary number, as there is no info about the text baseline
    switch (this.textbaseline) {
      case 'hanging':
      case 'top':
        offset.y = fontstyle.size / 1.75;
        break;
      case 'middle':
        break;
      default:
      case null:
      case 'alphabetic':
      case 'ideographic':
      case 'bottom':
        offset.y = -fontstyle.size / 2.25;
        break;
    }

    switch(textalign) {
      case 'right':
        left = delta;
        right = 0.05;
        break;
      case 'center':
        left = right = delta / 2;
        break;
    }

    var d = this.getcoords_(x + offset.x, y + offset.y);

    linestr.push('<g_vml_:line from="', -left ,' 0" to="', right ,' 0.05" ',
                 ' coordsize="100 100" coordorigin="0 0"',
                 ' filled="', !stroke, '" stroked="', !!stroke,
                 '" style="position:absolute;width:1px;height:1px;">');

    if (stroke) {
      appendstroke(this, linestr);
    } else {
      // todo: fix the min and max params.
      appendfill(this, linestr, {x: -left, y: 0},
                 {x: right, y: fontstyle.size});
    }

    var skewm = m[0][0].tofixed(3) + ',' + m[1][0].tofixed(3) + ',' +
                m[0][1].tofixed(3) + ',' + m[1][1].tofixed(3) + ',0,0';

    var skewoffset = mr(d.x / z) + ',' + mr(d.y / z);

    linestr.push('<g_vml_:skew on="t" matrix="', skewm ,'" ',
                 ' offset="', skewoffset, '" origin="', left ,' 0" />',
                 '<g_vml_:path textpathok="true" />',
                 '<g_vml_:textpath on="true" string="',
                 encodehtmlattribute(text),
                 '" style="v-text-align:', textalign,
                 ';font:', encodehtmlattribute(fontstylestring),
                 '" /></g_vml_:line>');

    this.element_.insertadjacenthtml('beforeend', linestr.join(''));
  };

  contextprototype.filltext = function(text, x, y, maxwidth) {
    this.drawtext_(text, x, y, maxwidth, false);
  };

  contextprototype.stroketext = function(text, x, y, maxwidth) {
    this.drawtext_(text, x, y, maxwidth, true);
  };

  contextprototype.measuretext = function(text) {
    if (!this.textmeasureel_) {
      var s = '<span style="position:absolute;' +
          'top:-20000px;left:0;padding:0;margin:0;border:none;' +
          'white-space:pre;"></span>';
      this.element_.insertadjacenthtml('beforeend', s);
      this.textmeasureel_ = this.element_.lastchild;
    }
    var doc = this.element_.ownerdocument;
    this.textmeasureel_.innerhtml = '';
    this.textmeasureel_.style.font = this.font;
    // don't use innerhtml or innertext because they allow markup/whitespace.
    this.textmeasureel_.appendchild(doc.createtextnode(text));
    return {width: this.textmeasureel_.offsetwidth};
  };

  /******** stubs ********/
  contextprototype.clip = function() {
    // todo: implement
  };

  contextprototype.arcto = function() {
    // todo: implement
  };

  contextprototype.createpattern = function(image, repetition) {
    return new canvaspattern_(image, repetition);
  };

  // gradient / pattern stubs
  function canvasgradient_(atype) {
    this.type_ = atype;
    this.x0_ = 0;
    this.y0_ = 0;
    this.r0_ = 0;
    this.x1_ = 0;
    this.y1_ = 0;
    this.r1_ = 0;
    this.colors_ = [];
  }

  canvasgradient_.prototype.addcolorstop = function(aoffset, acolor) {
    acolor = processstyle(acolor);
    this.colors_.push({offset: aoffset,
                       color: acolor.color,
                       alpha: acolor.alpha});
  };

  function canvaspattern_(image, repetition) {
    assertimageisvalid(image);
    switch (repetition) {
      case 'repeat':
      case null:
      case '':
        this.repetition_ = 'repeat';
        break
      case 'repeat-x':
      case 'repeat-y':
      case 'no-repeat':
        this.repetition_ = repetition;
        break;
      default:
        throwexception('syntax_err');
    }

    this.src_ = image.src;
    this.width_ = image.width;
    this.height_ = image.height;
  }

  function throwexception(s) {
    throw new domexception_(s);
  }

  function assertimageisvalid(img) {
    if (!img || img.nodetype != 1 || img.tagname != 'img') {
      throwexception('type_mismatch_err');
    }
    if (img.readystate != 'complete') {
      throwexception('invalid_state_err');
    }
  }

  function domexception_(s) {
    this.code = this[s];
    this.message = s +': dom exception ' + this.code;
  }
  var p = domexception_.prototype = new error;
  p.index_size_err = 1;
  p.domstring_size_err = 2;
  p.hierarchy_request_err = 3;
  p.wrong_document_err = 4;
  p.invalid_character_err = 5;
  p.no_data_allowed_err = 6;
  p.no_modification_allowed_err = 7;
  p.not_found_err = 8;
  p.not_supported_err = 9;
  p.inuse_attribute_err = 10;
  p.invalid_state_err = 11;
  p.syntax_err = 12;
  p.invalid_modification_err = 13;
  p.namespace_err = 14;
  p.invalid_access_err = 15;
  p.validation_err = 16;
  p.type_mismatch_err = 17;

  // set up externs
  g_vmlcanvasmanager = g_vmlcanvasmanager_;
  canvasrenderingcontext2d = canvasrenderingcontext2d_;
  canvasgradient = canvasgradient_;
  canvaspattern = canvaspattern_;
  domexception = domexception_;
})();

} // if
