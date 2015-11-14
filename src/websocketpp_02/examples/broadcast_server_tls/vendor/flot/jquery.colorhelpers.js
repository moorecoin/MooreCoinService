/* plugin for jquery for working with colors.
 * 
 * version 1.1.
 * 
 * inspiration from jquery color animation plugin by john resig.
 *
 * released under the mit license by ole laursen, october 2009.
 *
 * examples:
 *
 *   $.color.parse("#fff").scale('rgb', 0.25).add('a', -0.5).tostring()
 *   var c = $.color.extract($("#mydiv"), 'background-color');
 *   console.log(c.r, c.g, c.b, c.a);
 *   $.color.make(100, 50, 25, 0.4).tostring() // returns "rgba(100,50,25,0.4)"
 *
 * note that .scale() and .add() return the same modified object
 * instead of making a new one.
 *
 * v. 1.1: fix error handling so e.g. parsing an empty string does
 * produce a color rather than just crashing.
 */ 

(function($) {
    $.color = {};

    // construct color object with some convenient chainable helpers
    $.color.make = function (r, g, b, a) {
        var o = {};
        o.r = r || 0;
        o.g = g || 0;
        o.b = b || 0;
        o.a = a != null ? a : 1;

        o.add = function (c, d) {
            for (var i = 0; i < c.length; ++i)
                o[c.charat(i)] += d;
            return o.normalize();
        };
        
        o.scale = function (c, f) {
            for (var i = 0; i < c.length; ++i)
                o[c.charat(i)] *= f;
            return o.normalize();
        };
        
        o.tostring = function () {
            if (o.a >= 1.0) {
                return "rgb("+[o.r, o.g, o.b].join(",")+")";
            } else {
                return "rgba("+[o.r, o.g, o.b, o.a].join(",")+")";
            }
        };

        o.normalize = function () {
            function clamp(min, value, max) {
                return value < min ? min: (value > max ? max: value);
            }
            
            o.r = clamp(0, parseint(o.r), 255);
            o.g = clamp(0, parseint(o.g), 255);
            o.b = clamp(0, parseint(o.b), 255);
            o.a = clamp(0, o.a, 1);
            return o;
        };

        o.clone = function () {
            return $.color.make(o.r, o.b, o.g, o.a);
        };

        return o.normalize();
    }

    // extract css color property from element, going up in the dom
    // if it's "transparent"
    $.color.extract = function (elem, css) {
        var c;
        do {
            c = elem.css(css).tolowercase();
            // keep going until we find an element that has color, or
            // we hit the body
            if (c != '' && c != 'transparent')
                break;
            elem = elem.parent();
        } while (!$.nodename(elem.get(0), "body"));

        // catch safari's way of signalling transparent
        if (c == "rgba(0, 0, 0, 0)")
            c = "transparent";
        
        return $.color.parse(c);
    }
    
    // parse css color string (like "rgb(10, 32, 43)" or "#fff"),
    // returns color object, if parsing failed, you get black (0, 0,
    // 0) out
    $.color.parse = function (str) {
        var res, m = $.color.make;

        // look for rgb(num,num,num)
        if (res = /rgb\(\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*\)/.exec(str))
            return m(parseint(res[1], 10), parseint(res[2], 10), parseint(res[3], 10));
        
        // look for rgba(num,num,num,num)
        if (res = /rgba\(\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*,\s*([0-9]+(?:\.[0-9]+)?)\s*\)/.exec(str))
            return m(parseint(res[1], 10), parseint(res[2], 10), parseint(res[3], 10), parsefloat(res[4]));
            
        // look for rgb(num%,num%,num%)
        if (res = /rgb\(\s*([0-9]+(?:\.[0-9]+)?)\%\s*,\s*([0-9]+(?:\.[0-9]+)?)\%\s*,\s*([0-9]+(?:\.[0-9]+)?)\%\s*\)/.exec(str))
            return m(parsefloat(res[1])*2.55, parsefloat(res[2])*2.55, parsefloat(res[3])*2.55);

        // look for rgba(num%,num%,num%,num)
        if (res = /rgba\(\s*([0-9]+(?:\.[0-9]+)?)\%\s*,\s*([0-9]+(?:\.[0-9]+)?)\%\s*,\s*([0-9]+(?:\.[0-9]+)?)\%\s*,\s*([0-9]+(?:\.[0-9]+)?)\s*\)/.exec(str))
            return m(parsefloat(res[1])*2.55, parsefloat(res[2])*2.55, parsefloat(res[3])*2.55, parsefloat(res[4]));
        
        // look for #a0b1c2
        if (res = /#([a-fa-f0-9]{2})([a-fa-f0-9]{2})([a-fa-f0-9]{2})/.exec(str))
            return m(parseint(res[1], 16), parseint(res[2], 16), parseint(res[3], 16));

        // look for #fff
        if (res = /#([a-fa-f0-9])([a-fa-f0-9])([a-fa-f0-9])/.exec(str))
            return m(parseint(res[1]+res[1], 16), parseint(res[2]+res[2], 16), parseint(res[3]+res[3], 16));

        // otherwise, we're most likely dealing with a named color
        var name = $.trim(str).tolowercase();
        if (name == "transparent")
            return m(255, 255, 255, 0);
        else {
            // default to black
            res = lookupcolors[name] || [0, 0, 0];
            return m(res[0], res[1], res[2]);
        }
    }
    
    var lookupcolors = {
        aqua:[0,255,255],
        azure:[240,255,255],
        beige:[245,245,220],
        black:[0,0,0],
        blue:[0,0,255],
        brown:[165,42,42],
        cyan:[0,255,255],
        darkblue:[0,0,139],
        darkcyan:[0,139,139],
        darkgrey:[169,169,169],
        darkgreen:[0,100,0],
        darkkhaki:[189,183,107],
        darkmagenta:[139,0,139],
        darkolivegreen:[85,107,47],
        darkorange:[255,140,0],
        darkorchid:[153,50,204],
        darkred:[139,0,0],
        darksalmon:[233,150,122],
        darkviolet:[148,0,211],
        fuchsia:[255,0,255],
        gold:[255,215,0],
        green:[0,128,0],
        indigo:[75,0,130],
        khaki:[240,230,140],
        lightblue:[173,216,230],
        lightcyan:[224,255,255],
        lightgreen:[144,238,144],
        lightgrey:[211,211,211],
        lightpink:[255,182,193],
        lightyellow:[255,255,224],
        lime:[0,255,0],
        magenta:[255,0,255],
        maroon:[128,0,0],
        navy:[0,0,128],
        olive:[128,128,0],
        orange:[255,165,0],
        pink:[255,192,203],
        purple:[128,0,128],
        violet:[128,0,128],
        red:[255,0,0],
        silver:[192,192,192],
        white:[255,255,255],
        yellow:[255,255,0]
    };
})(jquery);
