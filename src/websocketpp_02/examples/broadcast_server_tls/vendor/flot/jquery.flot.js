/*! javascript plotting library for jquery, v. 0.7.
 *
 * released under the mit license by iola, december 2007.
 *
 */

// first an inline dependency, jquery.colorhelpers.js, we inline it here
// for convenience

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
(function(b){b.color={};b.color.make=function(f,e,c,d){var g={};g.r=f||0;g.g=e||0;g.b=c||0;g.a=d!=null?d:1;g.add=function(j,i){for(var h=0;h<j.length;++h){g[j.charat(h)]+=i}return g.normalize()};g.scale=function(j,i){for(var h=0;h<j.length;++h){g[j.charat(h)]*=i}return g.normalize()};g.tostring=function(){if(g.a>=1){return"rgb("+[g.r,g.g,g.b].join(",")+")"}else{return"rgba("+[g.r,g.g,g.b,g.a].join(",")+")"}};g.normalize=function(){function h(j,k,i){return k<j?j:(k>i?i:k)}g.r=h(0,parseint(g.r),255);g.g=h(0,parseint(g.g),255);g.b=h(0,parseint(g.b),255);g.a=h(0,g.a,1);return g};g.clone=function(){return b.color.make(g.r,g.b,g.g,g.a)};return g.normalize()};b.color.extract=function(d,c){var e;do{e=d.css(c).tolowercase();if(e!=""&&e!="transparent"){break}d=d.parent()}while(!b.nodename(d.get(0),"body"));if(e=="rgba(0, 0, 0, 0)"){e="transparent"}return b.color.parse(e)};b.color.parse=function(f){var e,c=b.color.make;if(e=/rgb\(\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*\)/.exec(f)){return c(parseint(e[1],10),parseint(e[2],10),parseint(e[3],10))}if(e=/rgba\(\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*,\s*([0-9]+(?:\.[0-9]+)?)\s*\)/.exec(f)){return c(parseint(e[1],10),parseint(e[2],10),parseint(e[3],10),parsefloat(e[4]))}if(e=/rgb\(\s*([0-9]+(?:\.[0-9]+)?)\%\s*,\s*([0-9]+(?:\.[0-9]+)?)\%\s*,\s*([0-9]+(?:\.[0-9]+)?)\%\s*\)/.exec(f)){return c(parsefloat(e[1])*2.55,parsefloat(e[2])*2.55,parsefloat(e[3])*2.55)}if(e=/rgba\(\s*([0-9]+(?:\.[0-9]+)?)\%\s*,\s*([0-9]+(?:\.[0-9]+)?)\%\s*,\s*([0-9]+(?:\.[0-9]+)?)\%\s*,\s*([0-9]+(?:\.[0-9]+)?)\s*\)/.exec(f)){return c(parsefloat(e[1])*2.55,parsefloat(e[2])*2.55,parsefloat(e[3])*2.55,parsefloat(e[4]))}if(e=/#([a-fa-f0-9]{2})([a-fa-f0-9]{2})([a-fa-f0-9]{2})/.exec(f)){return c(parseint(e[1],16),parseint(e[2],16),parseint(e[3],16))}if(e=/#([a-fa-f0-9])([a-fa-f0-9])([a-fa-f0-9])/.exec(f)){return c(parseint(e[1]+e[1],16),parseint(e[2]+e[2],16),parseint(e[3]+e[3],16))}var d=b.trim(f).tolowercase();if(d=="transparent"){return c(255,255,255,0)}else{e=a[d]||[0,0,0];return c(e[0],e[1],e[2])}};var a={aqua:[0,255,255],azure:[240,255,255],beige:[245,245,220],black:[0,0,0],blue:[0,0,255],brown:[165,42,42],cyan:[0,255,255],darkblue:[0,0,139],darkcyan:[0,139,139],darkgrey:[169,169,169],darkgreen:[0,100,0],darkkhaki:[189,183,107],darkmagenta:[139,0,139],darkolivegreen:[85,107,47],darkorange:[255,140,0],darkorchid:[153,50,204],darkred:[139,0,0],darksalmon:[233,150,122],darkviolet:[148,0,211],fuchsia:[255,0,255],gold:[255,215,0],green:[0,128,0],indigo:[75,0,130],khaki:[240,230,140],lightblue:[173,216,230],lightcyan:[224,255,255],lightgreen:[144,238,144],lightgrey:[211,211,211],lightpink:[255,182,193],lightyellow:[255,255,224],lime:[0,255,0],magenta:[255,0,255],maroon:[128,0,0],navy:[0,0,128],olive:[128,128,0],orange:[255,165,0],pink:[255,192,203],purple:[128,0,128],violet:[128,0,128],red:[255,0,0],silver:[192,192,192],white:[255,255,255],yellow:[255,255,0]}})(jquery);

// the actual flot code
(function($) {
    function plot(placeholder, data_, options_, plugins) {
        // data is on the form:
        //   [ series1, series2 ... ]
        // where series is either just the data as [ [x1, y1], [x2, y2], ... ]
        // or { data: [ [x1, y1], [x2, y2], ... ], label: "some label", ... }
        
        var series = [],
            options = {
                // the color theme used for graphs
                colors: ["#edc240", "#afd8f8", "#cb4b4b", "#4da74d", "#9440ed"],
                legend: {
                    show: true,
                    nocolumns: 1, // number of colums in legend table
                    labelformatter: null, // fn: string -> string
                    labelboxbordercolor: "#ccc", // border color for the little label boxes
                    container: null, // container (as jquery object) to put legend in, null means default on top of graph
                    position: "ne", // position of default legend container within plot
                    margin: 5, // distance from grid edge to default legend container within plot
                    backgroundcolor: null, // null means auto-detect
                    backgroundopacity: 0.85 // set to 0 to avoid background
                },
                xaxis: {
                    show: null, // null = auto-detect, true = always, false = never
                    position: "bottom", // or "top"
                    mode: null, // null or "time"
                    color: null, // base color, labels, ticks
                    tickcolor: null, // possibly different color of ticks, e.g. "rgba(0,0,0,0.15)"
                    transform: null, // null or f: number -> number to transform axis
                    inversetransform: null, // if transform is set, this should be the inverse function
                    min: null, // min. value to show, null means set automatically
                    max: null, // max. value to show, null means set automatically
                    autoscalemargin: null, // margin in % to add if auto-setting min/max
                    ticks: null, // either [1, 3] or [[1, "a"], 3] or (fn: axis info -> ticks) or app. number of ticks for auto-ticks
                    tickformatter: null, // fn: number -> string
                    labelwidth: null, // size of tick labels in pixels
                    labelheight: null,
                    reservespace: null, // whether to reserve space even if axis isn't shown
                    ticklength: null, // size in pixels of ticks, or "full" for whole line
                    aligntickswithaxis: null, // axis number or null for no sync
                    
                    // mode specific options
                    tickdecimals: null, // no. of decimals, null means auto
                    ticksize: null, // number or [number, "unit"]
                    minticksize: null, // number or [number, "unit"]
                    monthnames: null, // list of names of months
                    timeformat: null, // format string to use
                    twelvehourclock: false // 12 or 24 time in time mode
                },
                yaxis: {
                    autoscalemargin: 0.02,
                    position: "left" // or "right"
                },
                xaxes: [],
                yaxes: [],
                series: {
                    points: {
                        show: false,
                        radius: 3,
                        linewidth: 2, // in pixels
                        fill: true,
                        fillcolor: "#ffffff",
                        symbol: "circle" // or callback
                    },
                    lines: {
                        // we don't put in show: false so we can see
                        // whether lines were actively disabled 
                        linewidth: 2, // in pixels
                        fill: false,
                        fillcolor: null,
                        steps: false
                    },
                    bars: {
                        show: false,
                        linewidth: 2, // in pixels
                        barwidth: 1, // in units of the x axis
                        fill: true,
                        fillcolor: null,
                        align: "left", // or "center" 
                        horizontal: false
                    },
                    shadowsize: 3
                },
                grid: {
                    show: true,
                    abovedata: false,
                    color: "#545454", // primary color used for outline and labels
                    backgroundcolor: null, // null for transparent, else color
                    bordercolor: null, // set if different from the grid color
                    tickcolor: null, // color for the ticks, e.g. "rgba(0,0,0,0.15)"
                    labelmargin: 5, // in pixels
                    axismargin: 8, // in pixels
                    borderwidth: 2, // in pixels
                    minbordermargin: null, // in pixels, null means taken from points radius
                    markings: null, // array of ranges or fn: axes -> array of ranges
                    markingscolor: "#f4f4f4",
                    markingslinewidth: 2,
                    // interactive stuff
                    clickable: false,
                    hoverable: false,
                    autohighlight: true, // highlight in case mouse is near
                    mouseactiveradius: 10 // how far the mouse can be away to activate an item
                },
                hooks: {}
            },
        canvas = null,      // the canvas for the plot itself
        overlay = null,     // canvas for interactive stuff on top of plot
        eventholder = null, // jquery object that events should be bound to
        ctx = null, octx = null,
        xaxes = [], yaxes = [],
        plotoffset = { left: 0, right: 0, top: 0, bottom: 0},
        canvaswidth = 0, canvasheight = 0,
        plotwidth = 0, plotheight = 0,
        hooks = {
            processoptions: [],
            processrawdata: [],
            processdatapoints: [],
            drawseries: [],
            draw: [],
            bindevents: [],
            drawoverlay: [],
            shutdown: []
        },
        plot = this;

        // public functions
        plot.setdata = setdata;
        plot.setupgrid = setupgrid;
        plot.draw = draw;
        plot.getplaceholder = function() { return placeholder; };
        plot.getcanvas = function() { return canvas; };
        plot.getplotoffset = function() { return plotoffset; };
        plot.width = function () { return plotwidth; };
        plot.height = function () { return plotheight; };
        plot.offset = function () {
            var o = eventholder.offset();
            o.left += plotoffset.left;
            o.top += plotoffset.top;
            return o;
        };
        plot.getdata = function () { return series; };
        plot.getaxes = function () {
            var res = {}, i;
            $.each(xaxes.concat(yaxes), function (_, axis) {
                if (axis)
                    res[axis.direction + (axis.n != 1 ? axis.n : "") + "axis"] = axis;
            });
            return res;
        };
        plot.getxaxes = function () { return xaxes; };
        plot.getyaxes = function () { return yaxes; };
        plot.c2p = canvastoaxiscoords;
        plot.p2c = axistocanvascoords;
        plot.getoptions = function () { return options; };
        plot.highlight = highlight;
        plot.unhighlight = unhighlight;
        plot.triggerredrawoverlay = triggerredrawoverlay;
        plot.pointoffset = function(point) {
            return {
                left: parseint(xaxes[axisnumber(point, "x") - 1].p2c(+point.x) + plotoffset.left),
                top: parseint(yaxes[axisnumber(point, "y") - 1].p2c(+point.y) + plotoffset.top)
            };
        };
        plot.shutdown = shutdown;
        plot.resize = function () {
            getcanvasdimensions();
            resizecanvas(canvas);
            resizecanvas(overlay);
        };

        // public attributes
        plot.hooks = hooks;
        
        // initialize
        initplugins(plot);
        parseoptions(options_);
        setupcanvases();
        setdata(data_);
        setupgrid();
        draw();
        bindevents();


        function executehooks(hook, args) {
            args = [plot].concat(args);
            for (var i = 0; i < hook.length; ++i)
                hook[i].apply(this, args);
        }

        function initplugins() {
            for (var i = 0; i < plugins.length; ++i) {
                var p = plugins[i];
                p.init(plot);
                if (p.options)
                    $.extend(true, options, p.options);
            }
        }
        
        function parseoptions(opts) {
            var i;
            
            $.extend(true, options, opts);
            
            if (options.xaxis.color == null)
                options.xaxis.color = options.grid.color;
            if (options.yaxis.color == null)
                options.yaxis.color = options.grid.color;
            
            if (options.xaxis.tickcolor == null) // backwards-compatibility
                options.xaxis.tickcolor = options.grid.tickcolor;
            if (options.yaxis.tickcolor == null) // backwards-compatibility
                options.yaxis.tickcolor = options.grid.tickcolor;

            if (options.grid.bordercolor == null)
                options.grid.bordercolor = options.grid.color;
            if (options.grid.tickcolor == null)
                options.grid.tickcolor = $.color.parse(options.grid.color).scale('a', 0.22).tostring();
            
            // fill in defaults in axes, copy at least always the
            // first as the rest of the code assumes it'll be there
            for (i = 0; i < math.max(1, options.xaxes.length); ++i)
                options.xaxes[i] = $.extend(true, {}, options.xaxis, options.xaxes[i]);
            for (i = 0; i < math.max(1, options.yaxes.length); ++i)
                options.yaxes[i] = $.extend(true, {}, options.yaxis, options.yaxes[i]);

            // backwards compatibility, to be removed in future
            if (options.xaxis.noticks && options.xaxis.ticks == null)
                options.xaxis.ticks = options.xaxis.noticks;
            if (options.yaxis.noticks && options.yaxis.ticks == null)
                options.yaxis.ticks = options.yaxis.noticks;
            if (options.x2axis) {
                options.xaxes[1] = $.extend(true, {}, options.xaxis, options.x2axis);
                options.xaxes[1].position = "top";
            }
            if (options.y2axis) {
                options.yaxes[1] = $.extend(true, {}, options.yaxis, options.y2axis);
                options.yaxes[1].position = "right";
            }
            if (options.grid.coloredareas)
                options.grid.markings = options.grid.coloredareas;
            if (options.grid.coloredareascolor)
                options.grid.markingscolor = options.grid.coloredareascolor;
            if (options.lines)
                $.extend(true, options.series.lines, options.lines);
            if (options.points)
                $.extend(true, options.series.points, options.points);
            if (options.bars)
                $.extend(true, options.series.bars, options.bars);
            if (options.shadowsize != null)
                options.series.shadowsize = options.shadowsize;

            // save options on axes for future reference
            for (i = 0; i < options.xaxes.length; ++i)
                getorcreateaxis(xaxes, i + 1).options = options.xaxes[i];
            for (i = 0; i < options.yaxes.length; ++i)
                getorcreateaxis(yaxes, i + 1).options = options.yaxes[i];

            // add hooks from options
            for (var n in hooks)
                if (options.hooks[n] && options.hooks[n].length)
                    hooks[n] = hooks[n].concat(options.hooks[n]);

            executehooks(hooks.processoptions, [options]);
        }

        function setdata(d) {
            series = parsedata(d);
            fillinseriesoptions();
            processdata();
        }
        
        function parsedata(d) {
            var res = [];
            for (var i = 0; i < d.length; ++i) {
                var s = $.extend(true, {}, options.series);

                if (d[i].data != null) {
                    s.data = d[i].data; // move the data instead of deep-copy
                    delete d[i].data;

                    $.extend(true, s, d[i]);

                    d[i].data = s.data;
                }
                else
                    s.data = d[i];
                res.push(s);
            }

            return res;
        }
        
        function axisnumber(obj, coord) {
            var a = obj[coord + "axis"];
            if (typeof a == "object") // if we got a real axis, extract number
                a = a.n;
            if (typeof a != "number")
                a = 1; // default to first axis
            return a;
        }

        function allaxes() {
            // return flat array without annoying null entries
            return $.grep(xaxes.concat(yaxes), function (a) { return a; });
        }
        
        function canvastoaxiscoords(pos) {
            // return an object with x/y corresponding to all used axes 
            var res = {}, i, axis;
            for (i = 0; i < xaxes.length; ++i) {
                axis = xaxes[i];
                if (axis && axis.used)
                    res["x" + axis.n] = axis.c2p(pos.left);
            }

            for (i = 0; i < yaxes.length; ++i) {
                axis = yaxes[i];
                if (axis && axis.used)
                    res["y" + axis.n] = axis.c2p(pos.top);
            }
            
            if (res.x1 !== undefined)
                res.x = res.x1;
            if (res.y1 !== undefined)
                res.y = res.y1;

            return res;
        }
        
        function axistocanvascoords(pos) {
            // get canvas coords from the first pair of x/y found in pos
            var res = {}, i, axis, key;

            for (i = 0; i < xaxes.length; ++i) {
                axis = xaxes[i];
                if (axis && axis.used) {
                    key = "x" + axis.n;
                    if (pos[key] == null && axis.n == 1)
                        key = "x";

                    if (pos[key] != null) {
                        res.left = axis.p2c(pos[key]);
                        break;
                    }
                }
            }
            
            for (i = 0; i < yaxes.length; ++i) {
                axis = yaxes[i];
                if (axis && axis.used) {
                    key = "y" + axis.n;
                    if (pos[key] == null && axis.n == 1)
                        key = "y";

                    if (pos[key] != null) {
                        res.top = axis.p2c(pos[key]);
                        break;
                    }
                }
            }
            
            return res;
        }
        
        function getorcreateaxis(axes, number) {
            if (!axes[number - 1])
                axes[number - 1] = {
                    n: number, // save the number for future reference
                    direction: axes == xaxes ? "x" : "y",
                    options: $.extend(true, {}, axes == xaxes ? options.xaxis : options.yaxis)
                };
                
            return axes[number - 1];
        }

        function fillinseriesoptions() {
            var i;
            
            // collect what we already got of colors
            var neededcolors = series.length,
                usedcolors = [],
                assignedcolors = [];
            for (i = 0; i < series.length; ++i) {
                var sc = series[i].color;
                if (sc != null) {
                    --neededcolors;
                    if (typeof sc == "number")
                        assignedcolors.push(sc);
                    else
                        usedcolors.push($.color.parse(series[i].color));
                }
            }
            
            // we might need to generate more colors if higher indices
            // are assigned
            for (i = 0; i < assignedcolors.length; ++i) {
                neededcolors = math.max(neededcolors, assignedcolors[i] + 1);
            }

            // produce colors as needed
            var colors = [], variation = 0;
            i = 0;
            while (colors.length < neededcolors) {
                var c;
                if (options.colors.length == i) // check degenerate case
                    c = $.color.make(100, 100, 100);
                else
                    c = $.color.parse(options.colors[i]);

                // vary color if needed
                var sign = variation % 2 == 1 ? -1 : 1;
                c.scale('rgb', 1 + sign * math.ceil(variation / 2) * 0.2)

                // fixme: if we're getting to close to something else,
                // we should probably skip this one
                colors.push(c);
                
                ++i;
                if (i >= options.colors.length) {
                    i = 0;
                    ++variation;
                }
            }

            // fill in the options
            var colori = 0, s;
            for (i = 0; i < series.length; ++i) {
                s = series[i];
                
                // assign colors
                if (s.color == null) {
                    s.color = colors[colori].tostring();
                    ++colori;
                }
                else if (typeof s.color == "number")
                    s.color = colors[s.color].tostring();

                // turn on lines automatically in case nothing is set
                if (s.lines.show == null) {
                    var v, show = true;
                    for (v in s)
                        if (s[v] && s[v].show) {
                            show = false;
                            break;
                        }
                    if (show)
                        s.lines.show = true;
                }

                // setup axes
                s.xaxis = getorcreateaxis(xaxes, axisnumber(s, "x"));
                s.yaxis = getorcreateaxis(yaxes, axisnumber(s, "y"));
            }
        }
        
        function processdata() {
            var topsentry = number.positive_infinity,
                bottomsentry = number.negative_infinity,
                fakeinfinity = number.max_value,
                i, j, k, m, length,
                s, points, ps, x, y, axis, val, f, p;

            function updateaxis(axis, min, max) {
                if (min < axis.datamin && min != -fakeinfinity)
                    axis.datamin = min;
                if (max > axis.datamax && max != fakeinfinity)
                    axis.datamax = max;
            }

            $.each(allaxes(), function (_, axis) {
                // init axis
                axis.datamin = topsentry;
                axis.datamax = bottomsentry;
                axis.used = false;
            });
            
            for (i = 0; i < series.length; ++i) {
                s = series[i];
                s.datapoints = { points: [] };
                
                executehooks(hooks.processrawdata, [ s, s.data, s.datapoints ]);
            }
            
            // first pass: clean and copy data
            for (i = 0; i < series.length; ++i) {
                s = series[i];

                var data = s.data, format = s.datapoints.format;

                if (!format) {
                    format = [];
                    // find out how to copy
                    format.push({ x: true, number: true, required: true });
                    format.push({ y: true, number: true, required: true });

                    if (s.bars.show || (s.lines.show && s.lines.fill)) {
                        format.push({ y: true, number: true, required: false, defaultvalue: 0 });
                        if (s.bars.horizontal) {
                            delete format[format.length - 1].y;
                            format[format.length - 1].x = true;
                        }
                    }
                    
                    s.datapoints.format = format;
                }

                if (s.datapoints.pointsize != null)
                    continue; // already filled in

                s.datapoints.pointsize = format.length;
                
                ps = s.datapoints.pointsize;
                points = s.datapoints.points;

                insertsteps = s.lines.show && s.lines.steps;
                s.xaxis.used = s.yaxis.used = true;
                
                for (j = k = 0; j < data.length; ++j, k += ps) {
                    p = data[j];

                    var nullify = p == null;
                    if (!nullify) {
                        for (m = 0; m < ps; ++m) {
                            val = p[m];
                            f = format[m];

                            if (f) {
                                if (f.number && val != null) {
                                    val = +val; // convert to number
                                    if (isnan(val))
                                        val = null;
                                    else if (val == infinity)
                                        val = fakeinfinity;
                                    else if (val == -infinity)
                                        val = -fakeinfinity;
                                }

                                if (val == null) {
                                    if (f.required)
                                        nullify = true;
                                    
                                    if (f.defaultvalue != null)
                                        val = f.defaultvalue;
                                }
                            }
                            
                            points[k + m] = val;
                        }
                    }
                    
                    if (nullify) {
                        for (m = 0; m < ps; ++m) {
                            val = points[k + m];
                            if (val != null) {
                                f = format[m];
                                // extract min/max info
                                if (f.x)
                                    updateaxis(s.xaxis, val, val);
                                if (f.y)
                                    updateaxis(s.yaxis, val, val);
                            }
                            points[k + m] = null;
                        }
                    }
                    else {
                        // a little bit of line specific stuff that
                        // perhaps shouldn't be here, but lacking
                        // better means...
                        if (insertsteps && k > 0
                            && points[k - ps] != null
                            && points[k - ps] != points[k]
                            && points[k - ps + 1] != points[k + 1]) {
                            // copy the point to make room for a middle point
                            for (m = 0; m < ps; ++m)
                                points[k + ps + m] = points[k + m];

                            // middle point has same y
                            points[k + 1] = points[k - ps + 1];

                            // we've added a point, better reflect that
                            k += ps;
                        }
                    }
                }
            }

            // give the hooks a chance to run
            for (i = 0; i < series.length; ++i) {
                s = series[i];
                
                executehooks(hooks.processdatapoints, [ s, s.datapoints]);
            }

            // second pass: find datamax/datamin for auto-scaling
            for (i = 0; i < series.length; ++i) {
                s = series[i];
                points = s.datapoints.points,
                ps = s.datapoints.pointsize;

                var xmin = topsentry, ymin = topsentry,
                    xmax = bottomsentry, ymax = bottomsentry;
                
                for (j = 0; j < points.length; j += ps) {
                    if (points[j] == null)
                        continue;

                    for (m = 0; m < ps; ++m) {
                        val = points[j + m];
                        f = format[m];
                        if (!f || val == fakeinfinity || val == -fakeinfinity)
                            continue;
                        
                        if (f.x) {
                            if (val < xmin)
                                xmin = val;
                            if (val > xmax)
                                xmax = val;
                        }
                        if (f.y) {
                            if (val < ymin)
                                ymin = val;
                            if (val > ymax)
                                ymax = val;
                        }
                    }
                }
                
                if (s.bars.show) {
                    // make sure we got room for the bar on the dancing floor
                    var delta = s.bars.align == "left" ? 0 : -s.bars.barwidth/2;
                    if (s.bars.horizontal) {
                        ymin += delta;
                        ymax += delta + s.bars.barwidth;
                    }
                    else {
                        xmin += delta;
                        xmax += delta + s.bars.barwidth;
                    }
                }
                
                updateaxis(s.xaxis, xmin, xmax);
                updateaxis(s.yaxis, ymin, ymax);
            }

            $.each(allaxes(), function (_, axis) {
                if (axis.datamin == topsentry)
                    axis.datamin = null;
                if (axis.datamax == bottomsentry)
                    axis.datamax = null;
            });
        }

        function makecanvas(skippositioning, cls) {
            var c = document.createelement('canvas');
            c.classname = cls;
            c.width = canvaswidth;
            c.height = canvasheight;
                    
            if (!skippositioning)
                $(c).css({ position: 'absolute', left: 0, top: 0 });
                
            $(c).appendto(placeholder);
                
            if (!c.getcontext) // excanvas hack
                c = window.g_vmlcanvasmanager.initelement(c);

            // used for resetting in case we get replotted
            c.getcontext("2d").save();
            
            return c;
        }

        function getcanvasdimensions() {
            canvaswidth = placeholder.width();
            canvasheight = placeholder.height();
            
            if (canvaswidth <= 0 || canvasheight <= 0)
                throw "invalid dimensions for plot, width = " + canvaswidth + ", height = " + canvasheight;
        }

        function resizecanvas(c) {
            // resizing should reset the state (excanvas seems to be
            // buggy though)
            if (c.width != canvaswidth)
                c.width = canvaswidth;

            if (c.height != canvasheight)
                c.height = canvasheight;

            // so try to get back to the initial state (even if it's
            // gone now, this should be safe according to the spec)
            var cctx = c.getcontext("2d");
            cctx.restore();

            // and save again
            cctx.save();
        }
        
        function setupcanvases() {
            var reused,
                existingcanvas = placeholder.children("canvas.base"),
                existingoverlay = placeholder.children("canvas.overlay");

            if (existingcanvas.length == 0 || existingoverlay == 0) {
                // init everything
                
                placeholder.html(""); // make sure placeholder is clear
            
                placeholder.css({ padding: 0 }); // padding messes up the positioning
                
                if (placeholder.css("position") == 'static')
                    placeholder.css("position", "relative"); // for positioning labels and overlay

                getcanvasdimensions();
                
                canvas = makecanvas(true, "base");
                overlay = makecanvas(false, "overlay"); // overlay canvas for interactive features

                reused = false;
            }
            else {
                // reuse existing elements

                canvas = existingcanvas.get(0);
                overlay = existingoverlay.get(0);

                reused = true;
            }

            ctx = canvas.getcontext("2d");
            octx = overlay.getcontext("2d");

            // we include the canvas in the event holder too, because ie 7
            // sometimes has trouble with the stacking order
            eventholder = $([overlay, canvas]);

            if (reused) {
                // run shutdown in the old plot object
                placeholder.data("plot").shutdown();

                // reset reused canvases
                plot.resize();
                
                // make sure overlay pixels are cleared (canvas is cleared when we redraw)
                octx.clearrect(0, 0, canvaswidth, canvasheight);
                
                // then whack any remaining obvious garbage left
                eventholder.unbind();
                placeholder.children().not([canvas, overlay]).remove();
            }

            // save in case we get replotted
            placeholder.data("plot", plot);
        }

        function bindevents() {
            // bind events
            if (options.grid.hoverable) {
                eventholder.mousemove(onmousemove);
                eventholder.mouseleave(onmouseleave);
            }

            if (options.grid.clickable)
                eventholder.click(onclick);

            executehooks(hooks.bindevents, [eventholder]);
        }

        function shutdown() {
            if (redrawtimeout)
                cleartimeout(redrawtimeout);
            
            eventholder.unbind("mousemove", onmousemove);
            eventholder.unbind("mouseleave", onmouseleave);
            eventholder.unbind("click", onclick);
            
            executehooks(hooks.shutdown, [eventholder]);
        }

        function settransformationhelpers(axis) {
            // set helper functions on the axis, assumes plot area
            // has been computed already
            
            function identity(x) { return x; }
            
            var s, m, t = axis.options.transform || identity,
                it = axis.options.inversetransform;
            
            // precompute how much the axis is scaling a point
            // in canvas space
            if (axis.direction == "x") {
                s = axis.scale = plotwidth / math.abs(t(axis.max) - t(axis.min));
                m = math.min(t(axis.max), t(axis.min));
            }
            else {
                s = axis.scale = plotheight / math.abs(t(axis.max) - t(axis.min));
                s = -s;
                m = math.max(t(axis.max), t(axis.min));
            }

            // data point to canvas coordinate
            if (t == identity) // slight optimization
                axis.p2c = function (p) { return (p - m) * s; };
            else
                axis.p2c = function (p) { return (t(p) - m) * s; };
            // canvas coordinate to data point
            if (!it)
                axis.c2p = function (c) { return m + c / s; };
            else
                axis.c2p = function (c) { return it(m + c / s); };
        }

        function measureticklabels(axis) {
            var opts = axis.options, i, ticks = axis.ticks || [], labels = [],
                l, w = opts.labelwidth, h = opts.labelheight, dummydiv;

            function makedummydiv(labels, width) {
                return $('<div style="position:absolute;top:-10000px;' + width + 'font-size:smaller">' +
                         '<div class="' + axis.direction + 'axis ' + axis.direction + axis.n + 'axis">'
                         + labels.join("") + '</div></div>')
                    .appendto(placeholder);
            }
            
            if (axis.direction == "x") {
                // to avoid measuring the widths of the labels (it's slow), we
                // construct fixed-size boxes and put the labels inside
                // them, we don't need the exact figures and the
                // fixed-size box content is easy to center
                if (w == null)
                    w = math.floor(canvaswidth / (ticks.length > 0 ? ticks.length : 1));

                // measure x label heights
                if (h == null) {
                    labels = [];
                    for (i = 0; i < ticks.length; ++i) {
                        l = ticks[i].label;
                        if (l)
                            labels.push('<div class="ticklabel" style="float:left;width:' + w + 'px">' + l + '</div>');
                    }

                    if (labels.length > 0) {
                        // stick them all in the same div and measure
                        // collective height
                        labels.push('<div style="clear:left"></div>');
                        dummydiv = makedummydiv(labels, "width:10000px;");
                        h = dummydiv.height();
                        dummydiv.remove();
                    }
                }
            }
            else if (w == null || h == null) {
                // calculate y label dimensions
                for (i = 0; i < ticks.length; ++i) {
                    l = ticks[i].label;
                    if (l)
                        labels.push('<div class="ticklabel">' + l + '</div>');
                }
                
                if (labels.length > 0) {
                    dummydiv = makedummydiv(labels, "");
                    if (w == null)
                        w = dummydiv.children().width();
                    if (h == null)
                        h = dummydiv.find("div.ticklabel").height();
                    dummydiv.remove();
                }
            }

            if (w == null)
                w = 0;
            if (h == null)
                h = 0;

            axis.labelwidth = w;
            axis.labelheight = h;
        }

        function allocateaxisboxfirstphase(axis) {
            // find the bounding box of the axis by looking at label
            // widths/heights and ticks, make room by diminishing the
            // plotoffset

            var lw = axis.labelwidth,
                lh = axis.labelheight,
                pos = axis.options.position,
                ticklength = axis.options.ticklength,
                axismargin = options.grid.axismargin,
                padding = options.grid.labelmargin,
                all = axis.direction == "x" ? xaxes : yaxes,
                index;

            // determine axis margin
            var sameposition = $.grep(all, function (a) {
                return a && a.options.position == pos && a.reservespace;
            });
            if ($.inarray(axis, sameposition) == sameposition.length - 1)
                axismargin = 0; // outermost

            // determine tick length - if we're innermost, we can use "full"
            if (ticklength == null)
                ticklength = "full";

            var samedirection = $.grep(all, function (a) {
                return a && a.reservespace;
            });

            var innermost = $.inarray(axis, samedirection) == 0;
            if (!innermost && ticklength == "full")
                ticklength = 5;
                
            if (!isnan(+ticklength))
                padding += +ticklength;

            // compute box
            if (axis.direction == "x") {
                lh += padding;
                
                if (pos == "bottom") {
                    plotoffset.bottom += lh + axismargin;
                    axis.box = { top: canvasheight - plotoffset.bottom, height: lh };
                }
                else {
                    axis.box = { top: plotoffset.top + axismargin, height: lh };
                    plotoffset.top += lh + axismargin;
                }
            }
            else {
                lw += padding;
                
                if (pos == "left") {
                    axis.box = { left: plotoffset.left + axismargin, width: lw };
                    plotoffset.left += lw + axismargin;
                }
                else {
                    plotoffset.right += lw + axismargin;
                    axis.box = { left: canvaswidth - plotoffset.right, width: lw };
                }
            }

             // save for future reference
            axis.position = pos;
            axis.ticklength = ticklength;
            axis.box.padding = padding;
            axis.innermost = innermost;
        }

        function allocateaxisboxsecondphase(axis) {
            // set remaining bounding box coordinates
            if (axis.direction == "x") {
                axis.box.left = plotoffset.left;
                axis.box.width = plotwidth;
            }
            else {
                axis.box.top = plotoffset.top;
                axis.box.height = plotheight;
            }
        }
        
        function setupgrid() {
            var i, axes = allaxes();

            // first calculate the plot and axis box dimensions

            $.each(axes, function (_, axis) {
                axis.show = axis.options.show;
                if (axis.show == null)
                    axis.show = axis.used; // by default an axis is visible if it's got data
                
                axis.reservespace = axis.show || axis.options.reservespace;

                setrange(axis);
            });

            allocatedaxes = $.grep(axes, function (axis) { return axis.reservespace; });

            plotoffset.left = plotoffset.right = plotoffset.top = plotoffset.bottom = 0;
            if (options.grid.show) {
                $.each(allocatedaxes, function (_, axis) {
                    // make the ticks
                    setuptickgeneration(axis);
                    setticks(axis);
                    snaprangetoticks(axis, axis.ticks);

                    // find labelwidth/height for axis
                    measureticklabels(axis);
                });

                // with all dimensions in house, we can compute the
                // axis boxes, start from the outside (reverse order)
                for (i = allocatedaxes.length - 1; i >= 0; --i)
                    allocateaxisboxfirstphase(allocatedaxes[i]);

                // make sure we've got enough space for things that
                // might stick out
                var minmargin = options.grid.minbordermargin;
                if (minmargin == null) {
                    minmargin = 0;
                    for (i = 0; i < series.length; ++i)
                        minmargin = math.max(minmargin, series[i].points.radius + series[i].points.linewidth/2);
                }
                    
                for (var a in plotoffset) {
                    plotoffset[a] += options.grid.borderwidth;
                    plotoffset[a] = math.max(minmargin, plotoffset[a]);
                }
            }
            
            plotwidth = canvaswidth - plotoffset.left - plotoffset.right;
            plotheight = canvasheight - plotoffset.bottom - plotoffset.top;

            // now we got the proper plotwidth/height, we can compute the scaling
            $.each(axes, function (_, axis) {
                settransformationhelpers(axis);
            });

            if (options.grid.show) {
                $.each(allocatedaxes, function (_, axis) {
                    allocateaxisboxsecondphase(axis);
                });

                insertaxislabels();
            }
            
            insertlegend();
        }
        
        function setrange(axis) {
            var opts = axis.options,
                min = +(opts.min != null ? opts.min : axis.datamin),
                max = +(opts.max != null ? opts.max : axis.datamax),
                delta = max - min;

            if (delta == 0.0) {
                // degenerate case
                var widen = max == 0 ? 1 : 0.01;

                if (opts.min == null)
                    min -= widen;
                // always widen max if we couldn't widen min to ensure we
                // don't fall into min == max which doesn't work
                if (opts.max == null || opts.min != null)
                    max += widen;
            }
            else {
                // consider autoscaling
                var margin = opts.autoscalemargin;
                if (margin != null) {
                    if (opts.min == null) {
                        min -= delta * margin;
                        // make sure we don't go below zero if all values
                        // are positive
                        if (min < 0 && axis.datamin != null && axis.datamin >= 0)
                            min = 0;
                    }
                    if (opts.max == null) {
                        max += delta * margin;
                        if (max > 0 && axis.datamax != null && axis.datamax <= 0)
                            max = 0;
                    }
                }
            }
            axis.min = min;
            axis.max = max;
        }

        function setuptickgeneration(axis) {
            var opts = axis.options;
                
            // estimate number of ticks
            var noticks;
            if (typeof opts.ticks == "number" && opts.ticks > 0)
                noticks = opts.ticks;
            else
                // heuristic based on the model a*sqrt(x) fitted to
                // some data points that seemed reasonable
                noticks = 0.3 * math.sqrt(axis.direction == "x" ? canvaswidth : canvasheight);

            var delta = (axis.max - axis.min) / noticks,
                size, generator, unit, formatter, i, magn, norm;

            if (opts.mode == "time") {
                // pretty handling of time
                
                // map of app. size of time units in milliseconds
                var timeunitsize = {
                    "second": 1000,
                    "minute": 60 * 1000,
                    "hour": 60 * 60 * 1000,
                    "day": 24 * 60 * 60 * 1000,
                    "month": 30 * 24 * 60 * 60 * 1000,
                    "year": 365.2425 * 24 * 60 * 60 * 1000
                };


                // the allowed tick sizes, after 1 year we use
                // an integer algorithm
                var spec = [
                    [1, "second"], [2, "second"], [5, "second"], [10, "second"],
                    [30, "second"], 
                    [1, "minute"], [2, "minute"], [5, "minute"], [10, "minute"],
                    [30, "minute"], 
                    [1, "hour"], [2, "hour"], [4, "hour"],
                    [8, "hour"], [12, "hour"],
                    [1, "day"], [2, "day"], [3, "day"],
                    [0.25, "month"], [0.5, "month"], [1, "month"],
                    [2, "month"], [3, "month"], [6, "month"],
                    [1, "year"]
                ];

                var minsize = 0;
                if (opts.minticksize != null) {
                    if (typeof opts.ticksize == "number")
                        minsize = opts.ticksize;
                    else
                        minsize = opts.minticksize[0] * timeunitsize[opts.minticksize[1]];
                }

                for (var i = 0; i < spec.length - 1; ++i)
                    if (delta < (spec[i][0] * timeunitsize[spec[i][1]]
                                 + spec[i + 1][0] * timeunitsize[spec[i + 1][1]]) / 2
                       && spec[i][0] * timeunitsize[spec[i][1]] >= minsize)
                        break;
                size = spec[i][0];
                unit = spec[i][1];
                
                // special-case the possibility of several years
                if (unit == "year") {
                    magn = math.pow(10, math.floor(math.log(delta / timeunitsize.year) / math.ln10));
                    norm = (delta / timeunitsize.year) / magn;
                    if (norm < 1.5)
                        size = 1;
                    else if (norm < 3)
                        size = 2;
                    else if (norm < 7.5)
                        size = 5;
                    else
                        size = 10;

                    size *= magn;
                }

                axis.ticksize = opts.ticksize || [size, unit];
                
                generator = function(axis) {
                    var ticks = [],
                        ticksize = axis.ticksize[0], unit = axis.ticksize[1],
                        d = new date(axis.min);
                    
                    var step = ticksize * timeunitsize[unit];

                    if (unit == "second")
                        d.setutcseconds(floorinbase(d.getutcseconds(), ticksize));
                    if (unit == "minute")
                        d.setutcminutes(floorinbase(d.getutcminutes(), ticksize));
                    if (unit == "hour")
                        d.setutchours(floorinbase(d.getutchours(), ticksize));
                    if (unit == "month")
                        d.setutcmonth(floorinbase(d.getutcmonth(), ticksize));
                    if (unit == "year")
                        d.setutcfullyear(floorinbase(d.getutcfullyear(), ticksize));
                    
                    // reset smaller components
                    d.setutcmilliseconds(0);
                    if (step >= timeunitsize.minute)
                        d.setutcseconds(0);
                    if (step >= timeunitsize.hour)
                        d.setutcminutes(0);
                    if (step >= timeunitsize.day)
                        d.setutchours(0);
                    if (step >= timeunitsize.day * 4)
                        d.setutcdate(1);
                    if (step >= timeunitsize.year)
                        d.setutcmonth(0);


                    var carry = 0, v = number.nan, prev;
                    do {
                        prev = v;
                        v = d.gettime();
                        ticks.push(v);
                        if (unit == "month") {
                            if (ticksize < 1) {
                                // a bit complicated - we'll divide the month
                                // up but we need to take care of fractions
                                // so we don't end up in the middle of a day
                                d.setutcdate(1);
                                var start = d.gettime();
                                d.setutcmonth(d.getutcmonth() + 1);
                                var end = d.gettime();
                                d.settime(v + carry * timeunitsize.hour + (end - start) * ticksize);
                                carry = d.getutchours();
                                d.setutchours(0);
                            }
                            else
                                d.setutcmonth(d.getutcmonth() + ticksize);
                        }
                        else if (unit == "year") {
                            d.setutcfullyear(d.getutcfullyear() + ticksize);
                        }
                        else
                            d.settime(v + step);
                    } while (v < axis.max && v != prev);

                    return ticks;
                };

                formatter = function (v, axis) {
                    var d = new date(v);

                    // first check global format
                    if (opts.timeformat != null)
                        return $.plot.formatdate(d, opts.timeformat, opts.monthnames);
                    
                    var t = axis.ticksize[0] * timeunitsize[axis.ticksize[1]];
                    var span = axis.max - axis.min;
                    var suffix = (opts.twelvehourclock) ? " %p" : "";
                    
                    if (t < timeunitsize.minute)
                        fmt = "%h:%m:%s" + suffix;
                    else if (t < timeunitsize.day) {
                        if (span < 2 * timeunitsize.day)
                            fmt = "%h:%m" + suffix;
                        else
                            fmt = "%b %d %h:%m" + suffix;
                    }
                    else if (t < timeunitsize.month)
                        fmt = "%b %d";
                    else if (t < timeunitsize.year) {
                        if (span < timeunitsize.year)
                            fmt = "%b";
                        else
                            fmt = "%b %y";
                    }
                    else
                        fmt = "%y";
                    
                    return $.plot.formatdate(d, fmt, opts.monthnames);
                };
            }
            else {
                // pretty rounding of base-10 numbers
                var maxdec = opts.tickdecimals;
                var dec = -math.floor(math.log(delta) / math.ln10);
                if (maxdec != null && dec > maxdec)
                    dec = maxdec;

                magn = math.pow(10, -dec);
                norm = delta / magn; // norm is between 1.0 and 10.0
                
                if (norm < 1.5)
                    size = 1;
                else if (norm < 3) {
                    size = 2;
                    // special case for 2.5, requires an extra decimal
                    if (norm > 2.25 && (maxdec == null || dec + 1 <= maxdec)) {
                        size = 2.5;
                        ++dec;
                    }
                }
                else if (norm < 7.5)
                    size = 5;
                else
                    size = 10;

                size *= magn;
                
                if (opts.minticksize != null && size < opts.minticksize)
                    size = opts.minticksize;

                axis.tickdecimals = math.max(0, maxdec != null ? maxdec : dec);
                axis.ticksize = opts.ticksize || size;

                generator = function (axis) {
                    var ticks = [];

                    // spew out all possible ticks
                    var start = floorinbase(axis.min, axis.ticksize),
                        i = 0, v = number.nan, prev;
                    do {
                        prev = v;
                        v = start + i * axis.ticksize;
                        ticks.push(v);
                        ++i;
                    } while (v < axis.max && v != prev);
                    return ticks;
                };

                formatter = function (v, axis) {
                    return v.tofixed(axis.tickdecimals);
                };
            }

            if (opts.aligntickswithaxis != null) {
                var otheraxis = (axis.direction == "x" ? xaxes : yaxes)[opts.aligntickswithaxis - 1];
                if (otheraxis && otheraxis.used && otheraxis != axis) {
                    // consider snapping min/max to outermost nice ticks
                    var niceticks = generator(axis);
                    if (niceticks.length > 0) {
                        if (opts.min == null)
                            axis.min = math.min(axis.min, niceticks[0]);
                        if (opts.max == null && niceticks.length > 1)
                            axis.max = math.max(axis.max, niceticks[niceticks.length - 1]);
                    }
                    
                    generator = function (axis) {
                        // copy ticks, scaled to this axis
                        var ticks = [], v, i;
                        for (i = 0; i < otheraxis.ticks.length; ++i) {
                            v = (otheraxis.ticks[i].v - otheraxis.min) / (otheraxis.max - otheraxis.min);
                            v = axis.min + v * (axis.max - axis.min);
                            ticks.push(v);
                        }
                        return ticks;
                    };
                    
                    // we might need an extra decimal since forced
                    // ticks don't necessarily fit naturally
                    if (axis.mode != "time" && opts.tickdecimals == null) {
                        var extradec = math.max(0, -math.floor(math.log(delta) / math.ln10) + 1),
                            ts = generator(axis);

                        // only proceed if the tick interval rounded
                        // with an extra decimal doesn't give us a
                        // zero at end
                        if (!(ts.length > 1 && /\..*0$/.test((ts[1] - ts[0]).tofixed(extradec))))
                            axis.tickdecimals = extradec;
                    }
                }
            }

            axis.tickgenerator = generator;
            if ($.isfunction(opts.tickformatter))
                axis.tickformatter = function (v, axis) { return "" + opts.tickformatter(v, axis); };
            else
                axis.tickformatter = formatter;
        }
        
        function setticks(axis) {
            var oticks = axis.options.ticks, ticks = [];
            if (oticks == null || (typeof oticks == "number" && oticks > 0))
                ticks = axis.tickgenerator(axis);
            else if (oticks) {
                if ($.isfunction(oticks))
                    // generate the ticks
                    ticks = oticks({ min: axis.min, max: axis.max });
                else
                    ticks = oticks;
            }

            // clean up/labelify the supplied ticks, copy them over
            var i, v;
            axis.ticks = [];
            for (i = 0; i < ticks.length; ++i) {
                var label = null;
                var t = ticks[i];
                if (typeof t == "object") {
                    v = +t[0];
                    if (t.length > 1)
                        label = t[1];
                }
                else
                    v = +t;
                if (label == null)
                    label = axis.tickformatter(v, axis);
                if (!isnan(v))
                    axis.ticks.push({ v: v, label: label });
            }
        }

        function snaprangetoticks(axis, ticks) {
            if (axis.options.autoscalemargin && ticks.length > 0) {
                // snap to ticks
                if (axis.options.min == null)
                    axis.min = math.min(axis.min, ticks[0].v);
                if (axis.options.max == null && ticks.length > 1)
                    axis.max = math.max(axis.max, ticks[ticks.length - 1].v);
            }
        }
      
        function draw() {
            ctx.clearrect(0, 0, canvaswidth, canvasheight);

            var grid = options.grid;

            // draw background, if any
            if (grid.show && grid.backgroundcolor)
                drawbackground();
            
            if (grid.show && !grid.abovedata)
                drawgrid();

            for (var i = 0; i < series.length; ++i) {
                executehooks(hooks.drawseries, [ctx, series[i]]);
                drawseries(series[i]);
            }

            executehooks(hooks.draw, [ctx]);
            
            if (grid.show && grid.abovedata)
                drawgrid();
        }

        function extractrange(ranges, coord) {
            var axis, from, to, key, axes = allaxes();

            for (i = 0; i < axes.length; ++i) {
                axis = axes[i];
                if (axis.direction == coord) {
                    key = coord + axis.n + "axis";
                    if (!ranges[key] && axis.n == 1)
                        key = coord + "axis"; // support x1axis as xaxis
                    if (ranges[key]) {
                        from = ranges[key].from;
                        to = ranges[key].to;
                        break;
                    }
                }
            }

            // backwards-compat stuff - to be removed in future
            if (!ranges[key]) {
                axis = coord == "x" ? xaxes[0] : yaxes[0];
                from = ranges[coord + "1"];
                to = ranges[coord + "2"];
            }

            // auto-reverse as an added bonus
            if (from != null && to != null && from > to) {
                var tmp = from;
                from = to;
                to = tmp;
            }
            
            return { from: from, to: to, axis: axis };
        }
        
        function drawbackground() {
            ctx.save();
            ctx.translate(plotoffset.left, plotoffset.top);

            ctx.fillstyle = getcolororgradient(options.grid.backgroundcolor, plotheight, 0, "rgba(255, 255, 255, 0)");
            ctx.fillrect(0, 0, plotwidth, plotheight);
            ctx.restore();
        }

        function drawgrid() {
            var i;
            
            ctx.save();
            ctx.translate(plotoffset.left, plotoffset.top);

            // draw markings
            var markings = options.grid.markings;
            if (markings) {
                if ($.isfunction(markings)) {
                    var axes = plot.getaxes();
                    // xmin etc. is backwards compatibility, to be
                    // removed in the future
                    axes.xmin = axes.xaxis.min;
                    axes.xmax = axes.xaxis.max;
                    axes.ymin = axes.yaxis.min;
                    axes.ymax = axes.yaxis.max;
                    
                    markings = markings(axes);
                }

                for (i = 0; i < markings.length; ++i) {
                    var m = markings[i],
                        xrange = extractrange(m, "x"),
                        yrange = extractrange(m, "y");

                    // fill in missing
                    if (xrange.from == null)
                        xrange.from = xrange.axis.min;
                    if (xrange.to == null)
                        xrange.to = xrange.axis.max;
                    if (yrange.from == null)
                        yrange.from = yrange.axis.min;
                    if (yrange.to == null)
                        yrange.to = yrange.axis.max;

                    // clip
                    if (xrange.to < xrange.axis.min || xrange.from > xrange.axis.max ||
                        yrange.to < yrange.axis.min || yrange.from > yrange.axis.max)
                        continue;

                    xrange.from = math.max(xrange.from, xrange.axis.min);
                    xrange.to = math.min(xrange.to, xrange.axis.max);
                    yrange.from = math.max(yrange.from, yrange.axis.min);
                    yrange.to = math.min(yrange.to, yrange.axis.max);

                    if (xrange.from == xrange.to && yrange.from == yrange.to)
                        continue;

                    // then draw
                    xrange.from = xrange.axis.p2c(xrange.from);
                    xrange.to = xrange.axis.p2c(xrange.to);
                    yrange.from = yrange.axis.p2c(yrange.from);
                    yrange.to = yrange.axis.p2c(yrange.to);
                    
                    if (xrange.from == xrange.to || yrange.from == yrange.to) {
                        // draw line
                        ctx.beginpath();
                        ctx.strokestyle = m.color || options.grid.markingscolor;
                        ctx.linewidth = m.linewidth || options.grid.markingslinewidth;
                        ctx.moveto(xrange.from, yrange.from);
                        ctx.lineto(xrange.to, yrange.to);
                        ctx.stroke();
                    }
                    else {
                        // fill area
                        ctx.fillstyle = m.color || options.grid.markingscolor;
                        ctx.fillrect(xrange.from, yrange.to,
                                     xrange.to - xrange.from,
                                     yrange.from - yrange.to);
                    }
                }
            }
            
            // draw the ticks
            var axes = allaxes(), bw = options.grid.borderwidth;

            for (var j = 0; j < axes.length; ++j) {
                var axis = axes[j], box = axis.box,
                    t = axis.ticklength, x, y, xoff, yoff;
                if (!axis.show || axis.ticks.length == 0)
                    continue
                
                ctx.strokestyle = axis.options.tickcolor || $.color.parse(axis.options.color).scale('a', 0.22).tostring();
                ctx.linewidth = 1;

                // find the edges
                if (axis.direction == "x") {
                    x = 0;
                    if (t == "full")
                        y = (axis.position == "top" ? 0 : plotheight);
                    else
                        y = box.top - plotoffset.top + (axis.position == "top" ? box.height : 0);
                }
                else {
                    y = 0;
                    if (t == "full")
                        x = (axis.position == "left" ? 0 : plotwidth);
                    else
                        x = box.left - plotoffset.left + (axis.position == "left" ? box.width : 0);
                }
                
                // draw tick bar
                if (!axis.innermost) {
                    ctx.beginpath();
                    xoff = yoff = 0;
                    if (axis.direction == "x")
                        xoff = plotwidth;
                    else
                        yoff = plotheight;
                    
                    if (ctx.linewidth == 1) {
                        x = math.floor(x) + 0.5;
                        y = math.floor(y) + 0.5;
                    }

                    ctx.moveto(x, y);
                    ctx.lineto(x + xoff, y + yoff);
                    ctx.stroke();
                }

                // draw ticks
                ctx.beginpath();
                for (i = 0; i < axis.ticks.length; ++i) {
                    var v = axis.ticks[i].v;
                    
                    xoff = yoff = 0;

                    if (v < axis.min || v > axis.max
                        // skip those lying on the axes if we got a border
                        || (t == "full" && bw > 0
                            && (v == axis.min || v == axis.max)))
                        continue;

                    if (axis.direction == "x") {
                        x = axis.p2c(v);
                        yoff = t == "full" ? -plotheight : t;
                        
                        if (axis.position == "top")
                            yoff = -yoff;
                    }
                    else {
                        y = axis.p2c(v);
                        xoff = t == "full" ? -plotwidth : t;
                        
                        if (axis.position == "left")
                            xoff = -xoff;
                    }

                    if (ctx.linewidth == 1) {
                        if (axis.direction == "x")
                            x = math.floor(x) + 0.5;
                        else
                            y = math.floor(y) + 0.5;
                    }

                    ctx.moveto(x, y);
                    ctx.lineto(x + xoff, y + yoff);
                }
                
                ctx.stroke();
            }
            
            
            // draw border
            if (bw) {
                ctx.linewidth = bw;
                ctx.strokestyle = options.grid.bordercolor;
                ctx.strokerect(-bw/2, -bw/2, plotwidth + bw, plotheight + bw);
            }

            ctx.restore();
        }

        function insertaxislabels() {
            placeholder.find(".ticklabels").remove();
            
            var html = ['<div class="ticklabels" style="font-size:smaller">'];

            var axes = allaxes();
            for (var j = 0; j < axes.length; ++j) {
                var axis = axes[j], box = axis.box;
                if (!axis.show)
                    continue;
                //debug: html.push('<div style="position:absolute;opacity:0.10;background-color:red;left:' + box.left + 'px;top:' + box.top + 'px;width:' + box.width +  'px;height:' + box.height + 'px"></div>')
                html.push('<div class="' + axis.direction + 'axis ' + axis.direction + axis.n + 'axis" style="color:' + axis.options.color + '">');
                for (var i = 0; i < axis.ticks.length; ++i) {
                    var tick = axis.ticks[i];
                    if (!tick.label || tick.v < axis.min || tick.v > axis.max)
                        continue;

                    var pos = {}, align;
                    
                    if (axis.direction == "x") {
                        align = "center";
                        pos.left = math.round(plotoffset.left + axis.p2c(tick.v) - axis.labelwidth/2);
                        if (axis.position == "bottom")
                            pos.top = box.top + box.padding;
                        else
                            pos.bottom = canvasheight - (box.top + box.height - box.padding);
                    }
                    else {
                        pos.top = math.round(plotoffset.top + axis.p2c(tick.v) - axis.labelheight/2);
                        if (axis.position == "left") {
                            pos.right = canvaswidth - (box.left + box.width - box.padding)
                            align = "right";
                        }
                        else {
                            pos.left = box.left + box.padding;
                            align = "left";
                        }
                    }

                    pos.width = axis.labelwidth;

                    var style = ["position:absolute", "text-align:" + align ];
                    for (var a in pos)
                        style.push(a + ":" + pos[a] + "px")
                    
                    html.push('<div class="ticklabel" style="' + style.join(';') + '">' + tick.label + '</div>');
                }
                html.push('</div>');
            }

            html.push('</div>');

            placeholder.append(html.join(""));
        }

        function drawseries(series) {
            if (series.lines.show)
                drawserieslines(series);
            if (series.bars.show)
                drawseriesbars(series);
            if (series.points.show)
                drawseriespoints(series);
        }
        
        function drawserieslines(series) {
            function plotline(datapoints, xoffset, yoffset, axisx, axisy) {
                var points = datapoints.points,
                    ps = datapoints.pointsize,
                    prevx = null, prevy = null;
                
                ctx.beginpath();
                for (var i = ps; i < points.length; i += ps) {
                    var x1 = points[i - ps], y1 = points[i - ps + 1],
                        x2 = points[i], y2 = points[i + 1];
                    
                    if (x1 == null || x2 == null)
                        continue;

                    // clip with ymin
                    if (y1 <= y2 && y1 < axisy.min) {
                        if (y2 < axisy.min)
                            continue;   // line segment is outside
                        // compute new intersection point
                        x1 = (axisy.min - y1) / (y2 - y1) * (x2 - x1) + x1;
                        y1 = axisy.min;
                    }
                    else if (y2 <= y1 && y2 < axisy.min) {
                        if (y1 < axisy.min)
                            continue;
                        x2 = (axisy.min - y1) / (y2 - y1) * (x2 - x1) + x1;
                        y2 = axisy.min;
                    }

                    // clip with ymax
                    if (y1 >= y2 && y1 > axisy.max) {
                        if (y2 > axisy.max)
                            continue;
                        x1 = (axisy.max - y1) / (y2 - y1) * (x2 - x1) + x1;
                        y1 = axisy.max;
                    }
                    else if (y2 >= y1 && y2 > axisy.max) {
                        if (y1 > axisy.max)
                            continue;
                        x2 = (axisy.max - y1) / (y2 - y1) * (x2 - x1) + x1;
                        y2 = axisy.max;
                    }

                    // clip with xmin
                    if (x1 <= x2 && x1 < axisx.min) {
                        if (x2 < axisx.min)
                            continue;
                        y1 = (axisx.min - x1) / (x2 - x1) * (y2 - y1) + y1;
                        x1 = axisx.min;
                    }
                    else if (x2 <= x1 && x2 < axisx.min) {
                        if (x1 < axisx.min)
                            continue;
                        y2 = (axisx.min - x1) / (x2 - x1) * (y2 - y1) + y1;
                        x2 = axisx.min;
                    }

                    // clip with xmax
                    if (x1 >= x2 && x1 > axisx.max) {
                        if (x2 > axisx.max)
                            continue;
                        y1 = (axisx.max - x1) / (x2 - x1) * (y2 - y1) + y1;
                        x1 = axisx.max;
                    }
                    else if (x2 >= x1 && x2 > axisx.max) {
                        if (x1 > axisx.max)
                            continue;
                        y2 = (axisx.max - x1) / (x2 - x1) * (y2 - y1) + y1;
                        x2 = axisx.max;
                    }

                    if (x1 != prevx || y1 != prevy)
                        ctx.moveto(axisx.p2c(x1) + xoffset, axisy.p2c(y1) + yoffset);
                    
                    prevx = x2;
                    prevy = y2;
                    ctx.lineto(axisx.p2c(x2) + xoffset, axisy.p2c(y2) + yoffset);
                }
                ctx.stroke();
            }

            function plotlinearea(datapoints, axisx, axisy) {
                var points = datapoints.points,
                    ps = datapoints.pointsize,
                    bottom = math.min(math.max(0, axisy.min), axisy.max),
                    i = 0, top, areaopen = false,
                    ypos = 1, segmentstart = 0, segmentend = 0;

                // we process each segment in two turns, first forward
                // direction to sketch out top, then once we hit the
                // end we go backwards to sketch the bottom
                while (true) {
                    if (ps > 0 && i > points.length + ps)
                        break;

                    i += ps; // ps is negative if going backwards

                    var x1 = points[i - ps],
                        y1 = points[i - ps + ypos],
                        x2 = points[i], y2 = points[i + ypos];

                    if (areaopen) {
                        if (ps > 0 && x1 != null && x2 == null) {
                            // at turning point
                            segmentend = i;
                            ps = -ps;
                            ypos = 2;
                            continue;
                        }

                        if (ps < 0 && i == segmentstart + ps) {
                            // done with the reverse sweep
                            ctx.fill();
                            areaopen = false;
                            ps = -ps;
                            ypos = 1;
                            i = segmentstart = segmentend + ps;
                            continue;
                        }
                    }

                    if (x1 == null || x2 == null)
                        continue;

                    // clip x values
                    
                    // clip with xmin
                    if (x1 <= x2 && x1 < axisx.min) {
                        if (x2 < axisx.min)
                            continue;
                        y1 = (axisx.min - x1) / (x2 - x1) * (y2 - y1) + y1;
                        x1 = axisx.min;
                    }
                    else if (x2 <= x1 && x2 < axisx.min) {
                        if (x1 < axisx.min)
                            continue;
                        y2 = (axisx.min - x1) / (x2 - x1) * (y2 - y1) + y1;
                        x2 = axisx.min;
                    }

                    // clip with xmax
                    if (x1 >= x2 && x1 > axisx.max) {
                        if (x2 > axisx.max)
                            continue;
                        y1 = (axisx.max - x1) / (x2 - x1) * (y2 - y1) + y1;
                        x1 = axisx.max;
                    }
                    else if (x2 >= x1 && x2 > axisx.max) {
                        if (x1 > axisx.max)
                            continue;
                        y2 = (axisx.max - x1) / (x2 - x1) * (y2 - y1) + y1;
                        x2 = axisx.max;
                    }

                    if (!areaopen) {
                        // open area
                        ctx.beginpath();
                        ctx.moveto(axisx.p2c(x1), axisy.p2c(bottom));
                        areaopen = true;
                    }
                    
                    // now first check the case where both is outside
                    if (y1 >= axisy.max && y2 >= axisy.max) {
                        ctx.lineto(axisx.p2c(x1), axisy.p2c(axisy.max));
                        ctx.lineto(axisx.p2c(x2), axisy.p2c(axisy.max));
                        continue;
                    }
                    else if (y1 <= axisy.min && y2 <= axisy.min) {
                        ctx.lineto(axisx.p2c(x1), axisy.p2c(axisy.min));
                        ctx.lineto(axisx.p2c(x2), axisy.p2c(axisy.min));
                        continue;
                    }
                    
                    // else it's a bit more complicated, there might
                    // be a flat maxed out rectangle first, then a
                    // triangular cutout or reverse; to find these
                    // keep track of the current x values
                    var x1old = x1, x2old = x2;

                    // clip the y values, without shortcutting, we
                    // go through all cases in turn
                    
                    // clip with ymin
                    if (y1 <= y2 && y1 < axisy.min && y2 >= axisy.min) {
                        x1 = (axisy.min - y1) / (y2 - y1) * (x2 - x1) + x1;
                        y1 = axisy.min;
                    }
                    else if (y2 <= y1 && y2 < axisy.min && y1 >= axisy.min) {
                        x2 = (axisy.min - y1) / (y2 - y1) * (x2 - x1) + x1;
                        y2 = axisy.min;
                    }

                    // clip with ymax
                    if (y1 >= y2 && y1 > axisy.max && y2 <= axisy.max) {
                        x1 = (axisy.max - y1) / (y2 - y1) * (x2 - x1) + x1;
                        y1 = axisy.max;
                    }
                    else if (y2 >= y1 && y2 > axisy.max && y1 <= axisy.max) {
                        x2 = (axisy.max - y1) / (y2 - y1) * (x2 - x1) + x1;
                        y2 = axisy.max;
                    }

                    // if the x value was changed we got a rectangle
                    // to fill
                    if (x1 != x1old) {
                        ctx.lineto(axisx.p2c(x1old), axisy.p2c(y1));
                        // it goes to (x1, y1), but we fill that below
                    }
                    
                    // fill triangular section, this sometimes result
                    // in redundant points if (x1, y1) hasn't changed
                    // from previous line to, but we just ignore that
                    ctx.lineto(axisx.p2c(x1), axisy.p2c(y1));
                    ctx.lineto(axisx.p2c(x2), axisy.p2c(y2));

                    // fill the other rectangle if it's there
                    if (x2 != x2old) {
                        ctx.lineto(axisx.p2c(x2), axisy.p2c(y2));
                        ctx.lineto(axisx.p2c(x2old), axisy.p2c(y2));
                    }
                }
            }

            ctx.save();
            ctx.translate(plotoffset.left, plotoffset.top);
            ctx.linejoin = "round";

            var lw = series.lines.linewidth,
                sw = series.shadowsize;
            // fixme: consider another form of shadow when filling is turned on
            if (lw > 0 && sw > 0) {
                // draw shadow as a thick and thin line with transparency
                ctx.linewidth = sw;
                ctx.strokestyle = "rgba(0,0,0,0.1)";
                // position shadow at angle from the mid of line
                var angle = math.pi/18;
                plotline(series.datapoints, math.sin(angle) * (lw/2 + sw/2), math.cos(angle) * (lw/2 + sw/2), series.xaxis, series.yaxis);
                ctx.linewidth = sw/2;
                plotline(series.datapoints, math.sin(angle) * (lw/2 + sw/4), math.cos(angle) * (lw/2 + sw/4), series.xaxis, series.yaxis);
            }

            ctx.linewidth = lw;
            ctx.strokestyle = series.color;
            var fillstyle = getfillstyle(series.lines, series.color, 0, plotheight);
            if (fillstyle) {
                ctx.fillstyle = fillstyle;
                plotlinearea(series.datapoints, series.xaxis, series.yaxis);
            }

            if (lw > 0)
                plotline(series.datapoints, 0, 0, series.xaxis, series.yaxis);
            ctx.restore();
        }

        function drawseriespoints(series) {
            function plotpoints(datapoints, radius, fillstyle, offset, shadow, axisx, axisy, symbol) {
                var points = datapoints.points, ps = datapoints.pointsize;

                for (var i = 0; i < points.length; i += ps) {
                    var x = points[i], y = points[i + 1];
                    if (x == null || x < axisx.min || x > axisx.max || y < axisy.min || y > axisy.max)
                        continue;
                    
                    ctx.beginpath();
                    x = axisx.p2c(x);
                    y = axisy.p2c(y) + offset;
                    if (symbol == "circle")
                        ctx.arc(x, y, radius, 0, shadow ? math.pi : math.pi * 2, false);
                    else
                        symbol(ctx, x, y, radius, shadow);
                    ctx.closepath();
                    
                    if (fillstyle) {
                        ctx.fillstyle = fillstyle;
                        ctx.fill();
                    }
                    ctx.stroke();
                }
            }
            
            ctx.save();
            ctx.translate(plotoffset.left, plotoffset.top);

            var lw = series.points.linewidth,
                sw = series.shadowsize,
                radius = series.points.radius,
                symbol = series.points.symbol;
            if (lw > 0 && sw > 0) {
                // draw shadow in two steps
                var w = sw / 2;
                ctx.linewidth = w;
                ctx.strokestyle = "rgba(0,0,0,0.1)";
                plotpoints(series.datapoints, radius, null, w + w/2, true,
                           series.xaxis, series.yaxis, symbol);

                ctx.strokestyle = "rgba(0,0,0,0.2)";
                plotpoints(series.datapoints, radius, null, w/2, true,
                           series.xaxis, series.yaxis, symbol);
            }

            ctx.linewidth = lw;
            ctx.strokestyle = series.color;
            plotpoints(series.datapoints, radius,
                       getfillstyle(series.points, series.color), 0, false,
                       series.xaxis, series.yaxis, symbol);
            ctx.restore();
        }

        function drawbar(x, y, b, barleft, barright, offset, fillstylecallback, axisx, axisy, c, horizontal, linewidth) {
            var left, right, bottom, top,
                drawleft, drawright, drawtop, drawbottom,
                tmp;

            // in horizontal mode, we start the bar from the left
            // instead of from the bottom so it appears to be
            // horizontal rather than vertical
            if (horizontal) {
                drawbottom = drawright = drawtop = true;
                drawleft = false;
                left = b;
                right = x;
                top = y + barleft;
                bottom = y + barright;

                // account for negative bars
                if (right < left) {
                    tmp = right;
                    right = left;
                    left = tmp;
                    drawleft = true;
                    drawright = false;
                }
            }
            else {
                drawleft = drawright = drawtop = true;
                drawbottom = false;
                left = x + barleft;
                right = x + barright;
                bottom = b;
                top = y;

                // account for negative bars
                if (top < bottom) {
                    tmp = top;
                    top = bottom;
                    bottom = tmp;
                    drawbottom = true;
                    drawtop = false;
                }
            }
           
            // clip
            if (right < axisx.min || left > axisx.max ||
                top < axisy.min || bottom > axisy.max)
                return;
            
            if (left < axisx.min) {
                left = axisx.min;
                drawleft = false;
            }

            if (right > axisx.max) {
                right = axisx.max;
                drawright = false;
            }

            if (bottom < axisy.min) {
                bottom = axisy.min;
                drawbottom = false;
            }
            
            if (top > axisy.max) {
                top = axisy.max;
                drawtop = false;
            }

            left = axisx.p2c(left);
            bottom = axisy.p2c(bottom);
            right = axisx.p2c(right);
            top = axisy.p2c(top);
            
            // fill the bar
            if (fillstylecallback) {
                c.beginpath();
                c.moveto(left, bottom);
                c.lineto(left, top);
                c.lineto(right, top);
                c.lineto(right, bottom);
                c.fillstyle = fillstylecallback(bottom, top);
                c.fill();
            }

            // draw outline
            if (linewidth > 0 && (drawleft || drawright || drawtop || drawbottom)) {
                c.beginpath();

                // fixme: inline moveto is buggy with excanvas
                c.moveto(left, bottom + offset);
                if (drawleft)
                    c.lineto(left, top + offset);
                else
                    c.moveto(left, top + offset);
                if (drawtop)
                    c.lineto(right, top + offset);
                else
                    c.moveto(right, top + offset);
                if (drawright)
                    c.lineto(right, bottom + offset);
                else
                    c.moveto(right, bottom + offset);
                if (drawbottom)
                    c.lineto(left, bottom + offset);
                else
                    c.moveto(left, bottom + offset);
                c.stroke();
            }
        }
        
        function drawseriesbars(series) {
            function plotbars(datapoints, barleft, barright, offset, fillstylecallback, axisx, axisy) {
                var points = datapoints.points, ps = datapoints.pointsize;
                
                for (var i = 0; i < points.length; i += ps) {
                    if (points[i] == null)
                        continue;
                    drawbar(points[i], points[i + 1], points[i + 2], barleft, barright, offset, fillstylecallback, axisx, axisy, ctx, series.bars.horizontal, series.bars.linewidth);
                }
            }

            ctx.save();
            ctx.translate(plotoffset.left, plotoffset.top);

            // fixme: figure out a way to add shadows (for instance along the right edge)
            ctx.linewidth = series.bars.linewidth;
            ctx.strokestyle = series.color;
            var barleft = series.bars.align == "left" ? 0 : -series.bars.barwidth/2;
            var fillstylecallback = series.bars.fill ? function (bottom, top) { return getfillstyle(series.bars, series.color, bottom, top); } : null;
            plotbars(series.datapoints, barleft, barleft + series.bars.barwidth, 0, fillstylecallback, series.xaxis, series.yaxis);
            ctx.restore();
        }

        function getfillstyle(filloptions, seriescolor, bottom, top) {
            var fill = filloptions.fill;
            if (!fill)
                return null;

            if (filloptions.fillcolor)
                return getcolororgradient(filloptions.fillcolor, bottom, top, seriescolor);
            
            var c = $.color.parse(seriescolor);
            c.a = typeof fill == "number" ? fill : 0.4;
            c.normalize();
            return c.tostring();
        }
        
        function insertlegend() {
            placeholder.find(".legend").remove();

            if (!options.legend.show)
                return;
            
            var fragments = [], rowstarted = false,
                lf = options.legend.labelformatter, s, label;
            for (var i = 0; i < series.length; ++i) {
                s = series[i];
                label = s.label;
                if (!label)
                    continue;
                
                if (i % options.legend.nocolumns == 0) {
                    if (rowstarted)
                        fragments.push('</tr>');
                    fragments.push('<tr>');
                    rowstarted = true;
                }

                if (lf)
                    label = lf(label, s);
                
                fragments.push(
                    '<td class="legendcolorbox"><div style="border:1px solid ' + options.legend.labelboxbordercolor + ';padding:1px"><div style="width:4px;height:0;border:5px solid ' + s.color + ';overflow:hidden"></div></div></td>' +
                    '<td class="legendlabel">' + label + '</td>');
            }
            if (rowstarted)
                fragments.push('</tr>');
            
            if (fragments.length == 0)
                return;

            var table = '<table style="font-size:smaller;color:' + options.grid.color + '">' + fragments.join("") + '</table>';
            if (options.legend.container != null)
                $(options.legend.container).html(table);
            else {
                var pos = "",
                    p = options.legend.position,
                    m = options.legend.margin;
                if (m[0] == null)
                    m = [m, m];
                if (p.charat(0) == "n")
                    pos += 'top:' + (m[1] + plotoffset.top) + 'px;';
                else if (p.charat(0) == "s")
                    pos += 'bottom:' + (m[1] + plotoffset.bottom) + 'px;';
                if (p.charat(1) == "e")
                    pos += 'right:' + (m[0] + plotoffset.right) + 'px;';
                else if (p.charat(1) == "w")
                    pos += 'left:' + (m[0] + plotoffset.left) + 'px;';
                var legend = $('<div class="legend">' + table.replace('style="', 'style="position:absolute;' + pos +';') + '</div>').appendto(placeholder);
                if (options.legend.backgroundopacity != 0.0) {
                    // put in the transparent background
                    // separately to avoid blended labels and
                    // label boxes
                    var c = options.legend.backgroundcolor;
                    if (c == null) {
                        c = options.grid.backgroundcolor;
                        if (c && typeof c == "string")
                            c = $.color.parse(c);
                        else
                            c = $.color.extract(legend, 'background-color');
                        c.a = 1;
                        c = c.tostring();
                    }
                    var div = legend.children();
                    $('<div style="position:absolute;width:' + div.width() + 'px;height:' + div.height() + 'px;' + pos +'background-color:' + c + ';"> </div>').prependto(legend).css('opacity', options.legend.backgroundopacity);
                }
            }
        }


        // interactive features
        
        var highlights = [],
            redrawtimeout = null;
        
        // returns the data item the mouse is over, or null if none is found
        function findnearbyitem(mousex, mousey, seriesfilter) {
            var maxdistance = options.grid.mouseactiveradius,
                smallestdistance = maxdistance * maxdistance + 1,
                item = null, foundpoint = false, i, j;

            for (i = series.length - 1; i >= 0; --i) {
                if (!seriesfilter(series[i]))
                    continue;
                
                var s = series[i],
                    axisx = s.xaxis,
                    axisy = s.yaxis,
                    points = s.datapoints.points,
                    ps = s.datapoints.pointsize,
                    mx = axisx.c2p(mousex), // precompute some stuff to make the loop faster
                    my = axisy.c2p(mousey),
                    maxx = maxdistance / axisx.scale,
                    maxy = maxdistance / axisy.scale;

                // with inverse transforms, we can't use the maxx/maxy
                // optimization, sadly
                if (axisx.options.inversetransform)
                    maxx = number.max_value;
                if (axisy.options.inversetransform)
                    maxy = number.max_value;
                
                if (s.lines.show || s.points.show) {
                    for (j = 0; j < points.length; j += ps) {
                        var x = points[j], y = points[j + 1];
                        if (x == null)
                            continue;
                        
                        // for points and lines, the cursor must be within a
                        // certain distance to the data point
                        if (x - mx > maxx || x - mx < -maxx ||
                            y - my > maxy || y - my < -maxy)
                            continue;

                        // we have to calculate distances in pixels, not in
                        // data units, because the scales of the axes may be different
                        var dx = math.abs(axisx.p2c(x) - mousex),
                            dy = math.abs(axisy.p2c(y) - mousey),
                            dist = dx * dx + dy * dy; // we save the sqrt

                        // use <= to ensure last point takes precedence
                        // (last generally means on top of)
                        if (dist < smallestdistance) {
                            smallestdistance = dist;
                            item = [i, j / ps];
                        }
                    }
                }
                    
                if (s.bars.show && !item) { // no other point can be nearby
                    var barleft = s.bars.align == "left" ? 0 : -s.bars.barwidth/2,
                        barright = barleft + s.bars.barwidth;
                    
                    for (j = 0; j < points.length; j += ps) {
                        var x = points[j], y = points[j + 1], b = points[j + 2];
                        if (x == null)
                            continue;
  
                        // for a bar graph, the cursor must be inside the bar
                        if (series[i].bars.horizontal ? 
                            (mx <= math.max(b, x) && mx >= math.min(b, x) && 
                             my >= y + barleft && my <= y + barright) :
                            (mx >= x + barleft && mx <= x + barright &&
                             my >= math.min(b, y) && my <= math.max(b, y)))
                                item = [i, j / ps];
                    }
                }
            }

            if (item) {
                i = item[0];
                j = item[1];
                ps = series[i].datapoints.pointsize;
                
                return { datapoint: series[i].datapoints.points.slice(j * ps, (j + 1) * ps),
                         dataindex: j,
                         series: series[i],
                         seriesindex: i };
            }
            
            return null;
        }

        function onmousemove(e) {
            if (options.grid.hoverable)
                triggerclickhoverevent("plothover", e,
                                       function (s) { return s["hoverable"] != false; });
        }

        function onmouseleave(e) {
            if (options.grid.hoverable)
                triggerclickhoverevent("plothover", e,
                                       function (s) { return false; });
        }

        function onclick(e) {
            triggerclickhoverevent("plotclick", e,
                                   function (s) { return s["clickable"] != false; });
        }

        // trigger click or hover event (they send the same parameters
        // so we share their code)
        function triggerclickhoverevent(eventname, event, seriesfilter) {
            var offset = eventholder.offset(),
                canvasx = event.pagex - offset.left - plotoffset.left,
                canvasy = event.pagey - offset.top - plotoffset.top,
            pos = canvastoaxiscoords({ left: canvasx, top: canvasy });

            pos.pagex = event.pagex;
            pos.pagey = event.pagey;

            var item = findnearbyitem(canvasx, canvasy, seriesfilter);

            if (item) {
                // fill in mouse pos for any listeners out there
                item.pagex = parseint(item.series.xaxis.p2c(item.datapoint[0]) + offset.left + plotoffset.left);
                item.pagey = parseint(item.series.yaxis.p2c(item.datapoint[1]) + offset.top + plotoffset.top);
            }

            if (options.grid.autohighlight) {
                // clear auto-highlights
                for (var i = 0; i < highlights.length; ++i) {
                    var h = highlights[i];
                    if (h.auto == eventname &&
                        !(item && h.series == item.series &&
                          h.point[0] == item.datapoint[0] &&
                          h.point[1] == item.datapoint[1]))
                        unhighlight(h.series, h.point);
                }
                
                if (item)
                    highlight(item.series, item.datapoint, eventname);
            }
            
            placeholder.trigger(eventname, [ pos, item ]);
        }

        function triggerredrawoverlay() {
            if (!redrawtimeout)
                redrawtimeout = settimeout(drawoverlay, 30);
        }

        function drawoverlay() {
            redrawtimeout = null;

            // draw highlights
            octx.save();
            octx.clearrect(0, 0, canvaswidth, canvasheight);
            octx.translate(plotoffset.left, plotoffset.top);
            
            var i, hi;
            for (i = 0; i < highlights.length; ++i) {
                hi = highlights[i];

                if (hi.series.bars.show)
                    drawbarhighlight(hi.series, hi.point);
                else
                    drawpointhighlight(hi.series, hi.point);
            }
            octx.restore();
            
            executehooks(hooks.drawoverlay, [octx]);
        }
        
        function highlight(s, point, auto) {
            if (typeof s == "number")
                s = series[s];

            if (typeof point == "number") {
                var ps = s.datapoints.pointsize;
                point = s.datapoints.points.slice(ps * point, ps * (point + 1));
            }

            var i = indexofhighlight(s, point);
            if (i == -1) {
                highlights.push({ series: s, point: point, auto: auto });

                triggerredrawoverlay();
            }
            else if (!auto)
                highlights[i].auto = false;
        }
            
        function unhighlight(s, point) {
            if (s == null && point == null) {
                highlights = [];
                triggerredrawoverlay();
            }
            
            if (typeof s == "number")
                s = series[s];

            if (typeof point == "number")
                point = s.data[point];

            var i = indexofhighlight(s, point);
            if (i != -1) {
                highlights.splice(i, 1);

                triggerredrawoverlay();
            }
        }
        
        function indexofhighlight(s, p) {
            for (var i = 0; i < highlights.length; ++i) {
                var h = highlights[i];
                if (h.series == s && h.point[0] == p[0]
                    && h.point[1] == p[1])
                    return i;
            }
            return -1;
        }
        
        function drawpointhighlight(series, point) {
            var x = point[0], y = point[1],
                axisx = series.xaxis, axisy = series.yaxis;
            
            if (x < axisx.min || x > axisx.max || y < axisy.min || y > axisy.max)
                return;
            
            var pointradius = series.points.radius + series.points.linewidth / 2;
            octx.linewidth = pointradius;
            octx.strokestyle = $.color.parse(series.color).scale('a', 0.5).tostring();
            var radius = 1.5 * pointradius,
                x = axisx.p2c(x),
                y = axisy.p2c(y);
            
            octx.beginpath();
            if (series.points.symbol == "circle")
                octx.arc(x, y, radius, 0, 2 * math.pi, false);
            else
                series.points.symbol(octx, x, y, radius, false);
            octx.closepath();
            octx.stroke();
        }

        function drawbarhighlight(series, point) {
            octx.linewidth = series.bars.linewidth;
            octx.strokestyle = $.color.parse(series.color).scale('a', 0.5).tostring();
            var fillstyle = $.color.parse(series.color).scale('a', 0.5).tostring();
            var barleft = series.bars.align == "left" ? 0 : -series.bars.barwidth/2;
            drawbar(point[0], point[1], point[2] || 0, barleft, barleft + series.bars.barwidth,
                    0, function () { return fillstyle; }, series.xaxis, series.yaxis, octx, series.bars.horizontal, series.bars.linewidth);
        }

        function getcolororgradient(spec, bottom, top, defaultcolor) {
            if (typeof spec == "string")
                return spec;
            else {
                // assume this is a gradient spec; ie currently only
                // supports a simple vertical gradient properly, so that's
                // what we support too
                var gradient = ctx.createlineargradient(0, top, 0, bottom);
                
                for (var i = 0, l = spec.colors.length; i < l; ++i) {
                    var c = spec.colors[i];
                    if (typeof c != "string") {
                        var co = $.color.parse(defaultcolor);
                        if (c.brightness != null)
                            co = co.scale('rgb', c.brightness)
                        if (c.opacity != null)
                            co.a *= c.opacity;
                        c = co.tostring();
                    }
                    gradient.addcolorstop(i / (l - 1), c);
                }
                
                return gradient;
            }
        }
    }

    $.plot = function(placeholder, data, options) {
        //var t0 = new date();
        var plot = new plot($(placeholder), data, options, $.plot.plugins);
        //(window.console ? console.log : alert)("time used (msecs): " + ((new date()).gettime() - t0.gettime()));
        return plot;
    };

    $.plot.version = "0.7";
    
    $.plot.plugins = [];

    // returns a string with the date d formatted according to fmt
    $.plot.formatdate = function(d, fmt, monthnames) {
        var leftpad = function(n) {
            n = "" + n;
            return n.length == 1 ? "0" + n : n;
        };
        
        var r = [];
        var escape = false, padnext = false;
        var hours = d.getutchours();
        var isam = hours < 12;
        if (monthnames == null)
            monthnames = ["jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"];

        if (fmt.search(/%p|%p/) != -1) {
            if (hours > 12) {
                hours = hours - 12;
            } else if (hours == 0) {
                hours = 12;
            }
        }
        for (var i = 0; i < fmt.length; ++i) {
            var c = fmt.charat(i);
            
            if (escape) {
                switch (c) {
                case 'h': c = "" + hours; break;
                case 'h': c = leftpad(hours); break;
                case 'm': c = leftpad(d.getutcminutes()); break;
                case 's': c = leftpad(d.getutcseconds()); break;
                case 'd': c = "" + d.getutcdate(); break;
                case 'm': c = "" + (d.getutcmonth() + 1); break;
                case 'y': c = "" + d.getutcfullyear(); break;
                case 'b': c = "" + monthnames[d.getutcmonth()]; break;
                case 'p': c = (isam) ? ("" + "am") : ("" + "pm"); break;
                case 'p': c = (isam) ? ("" + "am") : ("" + "pm"); break;
                case '0': c = ""; padnext = true; break;
                }
                if (c && padnext) {
                    c = leftpad(c);
                    padnext = false;
                }
                r.push(c);
                if (!padnext)
                    escape = false;
            }
            else {
                if (c == "%")
                    escape = true;
                else
                    r.push(c);
            }
        }
        return r.join("");
    };
    
    // round to nearby lower multiple of base
    function floorinbase(n, base) {
        return base * math.floor(n / base);
    }
    
})(jquery);
