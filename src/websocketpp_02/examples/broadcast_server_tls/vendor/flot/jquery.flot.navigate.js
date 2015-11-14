/*
flot plugin for adding panning and zooming capabilities to a plot.

the default behaviour is double click and scrollwheel up/down to zoom
in, drag to pan. the plugin defines plot.zoom({ center }),
plot.zoomout() and plot.pan(offset) so you easily can add custom
controls. it also fires a "plotpan" and "plotzoom" event when
something happens, useful for synchronizing plots.

options:

  zoom: {
    interactive: false
    trigger: "dblclick" // or "click" for single click
    amount: 1.5         // 2 = 200% (zoom in), 0.5 = 50% (zoom out)
  }
  
  pan: {
    interactive: false
    cursor: "move"      // css mouse cursor value used when dragging, e.g. "pointer"
    framerate: 20
  }

  xaxis, yaxis, x2axis, y2axis: {
    zoomrange: null  // or [number, number] (min range, max range) or false
    panrange: null   // or [number, number] (min, max) or false
  }
  
"interactive" enables the built-in drag/click behaviour. if you enable
interactive for pan, then you'll have a basic plot that supports
moving around; the same for zoom.

"amount" specifies the default amount to zoom in (so 1.5 = 150%)
relative to the current viewport.

"cursor" is a standard css mouse cursor string used for visual
feedback to the user when dragging.

"framerate" specifies the maximum number of times per second the plot
will update itself while the user is panning around on it (set to null
to disable intermediate pans, the plot will then not update until the
mouse button is released).

"zoomrange" is the interval in which zooming can happen, e.g. with
zoomrange: [1, 100] the zoom will never scale the axis so that the
difference between min and max is smaller than 1 or larger than 100.
you can set either end to null to ignore, e.g. [1, null]. if you set
zoomrange to false, zooming on that axis will be disabled.

"panrange" confines the panning to stay within a range, e.g. with
panrange: [-10, 20] panning stops at -10 in one end and at 20 in the
other. either can be null, e.g. [-10, null]. if you set
panrange to false, panning on that axis will be disabled.

example api usage:

  plot = $.plot(...);
  
  // zoom default amount in on the pixel (10, 20) 
  plot.zoom({ center: { left: 10, top: 20 } });

  // zoom out again
  plot.zoomout({ center: { left: 10, top: 20 } });

  // zoom 200% in on the pixel (10, 20) 
  plot.zoom({ amount: 2, center: { left: 10, top: 20 } });
  
  // pan 100 pixels to the left and 20 down
  plot.pan({ left: -100, top: 20 })

here, "center" specifies where the center of the zooming should
happen. note that this is defined in pixel space, not the space of the
data points (you can use the p2c helpers on the axes in flot to help
you convert between these).

"amount" is the amount to zoom the viewport relative to the current
range, so 1 is 100% (i.e. no change), 1.5 is 150% (zoom in), 0.7 is
70% (zoom out). you can set the default in the options.
  
*/


// first two dependencies, jquery.event.drag.js and
// jquery.mousewheel.js, we put them inline here to save people the
// effort of downloading them.

/*
jquery.event.drag.js ~ v1.5 ~ copyright (c) 2008, three dub media (http://threedubmedia.com)  
licensed under the mit license ~ http://threedubmedia.googlecode.com/files/mit-license.txt
*/
(function(e){e.fn.drag=function(l,k,j){if(k){this.bind("dragstart",l)}if(j){this.bind("dragend",j)}return !l?this.trigger("drag"):this.bind("drag",k?k:l)};var a=e.event,b=a.special,f=b.drag={not:":input",distance:0,which:1,dragging:false,setup:function(j){j=e.extend({distance:f.distance,which:f.which,not:f.not},j||{});j.distance=i(j.distance);a.add(this,"mousedown",h,j);if(this.attachevent){this.attachevent("ondragstart",d)}},teardown:function(){a.remove(this,"mousedown",h);if(this===f.dragging){f.dragging=f.proxy=false}g(this,true);if(this.detachevent){this.detachevent("ondragstart",d)}}};b.dragstart=b.dragend={setup:function(){},teardown:function(){}};function h(l){var k=this,j,m=l.data||{};if(m.elem){k=l.dragtarget=m.elem;l.dragproxy=f.proxy||k;l.cursoroffsetx=m.pagex-m.left;l.cursoroffsety=m.pagey-m.top;l.offsetx=l.pagex-l.cursoroffsetx;l.offsety=l.pagey-l.cursoroffsety}else{if(f.dragging||(m.which>0&&l.which!=m.which)||e(l.target).is(m.not)){return }}switch(l.type){case"mousedown":e.extend(m,e(k).offset(),{elem:k,target:l.target,pagex:l.pagex,pagey:l.pagey});a.add(document,"mousemove mouseup",h,m);g(k,false);f.dragging=null;return false;case !f.dragging&&"mousemove":if(i(l.pagex-m.pagex)+i(l.pagey-m.pagey)<m.distance){break}l.target=m.target;j=c(l,"dragstart",k);if(j!==false){f.dragging=k;f.proxy=l.dragproxy=e(j||k)[0]}case"mousemove":if(f.dragging){j=c(l,"drag",k);if(b.drop){b.drop.allowed=(j!==false);b.drop.handler(l)}if(j!==false){break}l.type="mouseup"}case"mouseup":a.remove(document,"mousemove mouseup",h);if(f.dragging){if(b.drop){b.drop.handler(l)}c(l,"dragend",k)}g(k,true);f.dragging=f.proxy=m.elem=false;break}return true}function c(m,k,l){m.type=k;var j=e.event.handle.call(l,m);return j===false?false:j||m.result}function i(j){return math.pow(j,2)}function d(){return(f.dragging===false)}function g(k,j){if(!k){return }k.unselectable=j?"off":"on";k.onselectstart=function(){return j};if(k.style){k.style.mozuserselect=j?"":"none"}}})(jquery);


/* jquery.mousewheel.min.js
 * copyright (c) 2009 brandon aaron (http://brandonaaron.net)
 * dual licensed under the mit (http://www.opensource.org/licenses/mit-license.php)
 * and gpl (http://www.opensource.org/licenses/gpl-license.php) licenses.
 * thanks to: http://adomas.org/javascript-mouse-wheel/ for some pointers.
 * thanks to: mathias bank(http://www.mathias-bank.de) for a scope bug fix.
 *
 * version: 3.0.2
 * 
 * requires: 1.2.2+
 */
(function(c){var a=["dommousescroll","mousewheel"];c.event.special.mousewheel={setup:function(){if(this.addeventlistener){for(var d=a.length;d;){this.addeventlistener(a[--d],b,false)}}else{this.onmousewheel=b}},teardown:function(){if(this.removeeventlistener){for(var d=a.length;d;){this.removeeventlistener(a[--d],b,false)}}else{this.onmousewheel=null}}};c.fn.extend({mousewheel:function(d){return d?this.bind("mousewheel",d):this.trigger("mousewheel")},unmousewheel:function(d){return this.unbind("mousewheel",d)}});function b(f){var d=[].slice.call(arguments,1),g=0,e=true;f=c.event.fix(f||window.event);f.type="mousewheel";if(f.wheeldelta){g=f.wheeldelta/120}if(f.detail){g=-f.detail/3}d.unshift(f,g);return c.event.handle.apply(this,d)}})(jquery);




(function ($) {
    var options = {
        xaxis: {
            zoomrange: null, // or [number, number] (min range, max range)
            panrange: null // or [number, number] (min, max)
        },
        zoom: {
            interactive: false,
            trigger: "dblclick", // or "click" for single click
            amount: 1.5 // how much to zoom relative to current position, 2 = 200% (zoom in), 0.5 = 50% (zoom out)
        },
        pan: {
            interactive: false,
            cursor: "move",
            framerate: 20
        }
    };

    function init(plot) {
        function onzoomclick(e, zoomout) {
            var c = plot.offset();
            c.left = e.pagex - c.left;
            c.top = e.pagey - c.top;
            if (zoomout)
                plot.zoomout({ center: c });
            else
                plot.zoom({ center: c });
        }

        function onmousewheel(e, delta) {
            onzoomclick(e, delta < 0);
            return false;
        }
        
        var prevcursor = 'default', prevpagex = 0, prevpagey = 0,
            pantimeout = null;

        function ondragstart(e) {
            if (e.which != 1)  // only accept left-click
                return false;
            var c = plot.getplaceholder().css('cursor');
            if (c)
                prevcursor = c;
            plot.getplaceholder().css('cursor', plot.getoptions().pan.cursor);
            prevpagex = e.pagex;
            prevpagey = e.pagey;
        }
        
        function ondrag(e) {
            var framerate = plot.getoptions().pan.framerate;
            if (pantimeout || !framerate)
                return;

            pantimeout = settimeout(function () {
                plot.pan({ left: prevpagex - e.pagex,
                           top: prevpagey - e.pagey });
                prevpagex = e.pagex;
                prevpagey = e.pagey;
                                                    
                pantimeout = null;
            }, 1 / framerate * 1000);
        }

        function ondragend(e) {
            if (pantimeout) {
                cleartimeout(pantimeout);
                pantimeout = null;
            }
                    
            plot.getplaceholder().css('cursor', prevcursor);
            plot.pan({ left: prevpagex - e.pagex,
                       top: prevpagey - e.pagey });
        }
        
        function bindevents(plot, eventholder) {
            var o = plot.getoptions();
            if (o.zoom.interactive) {
                eventholder[o.zoom.trigger](onzoomclick);
                eventholder.mousewheel(onmousewheel);
            }

            if (o.pan.interactive) {
                eventholder.bind("dragstart", { distance: 10 }, ondragstart);
                eventholder.bind("drag", ondrag);
                eventholder.bind("dragend", ondragend);
            }
        }

        plot.zoomout = function (args) {
            if (!args)
                args = {};
            
            if (!args.amount)
                args.amount = plot.getoptions().zoom.amount

            args.amount = 1 / args.amount;
            plot.zoom(args);
        }
        
        plot.zoom = function (args) {
            if (!args)
                args = {};
            
            var c = args.center,
                amount = args.amount || plot.getoptions().zoom.amount,
                w = plot.width(), h = plot.height();

            if (!c)
                c = { left: w / 2, top: h / 2 };
                
            var xf = c.left / w,
                yf = c.top / h,
                minmax = {
                    x: {
                        min: c.left - xf * w / amount,
                        max: c.left + (1 - xf) * w / amount
                    },
                    y: {
                        min: c.top - yf * h / amount,
                        max: c.top + (1 - yf) * h / amount
                    }
                };

            $.each(plot.getaxes(), function(_, axis) {
                var opts = axis.options,
                    min = minmax[axis.direction].min,
                    max = minmax[axis.direction].max,
                    zr = opts.zoomrange;

                if (zr === false) // no zooming on this axis
                    return;
                    
                min = axis.c2p(min);
                max = axis.c2p(max);
                if (min > max) {
                    // make sure min < max
                    var tmp = min;
                    min = max;
                    max = tmp;
                }

                var range = max - min;
                if (zr &&
                    ((zr[0] != null && range < zr[0]) ||
                     (zr[1] != null && range > zr[1])))
                    return;
            
                opts.min = min;
                opts.max = max;
            });
            
            plot.setupgrid();
            plot.draw();
            
            if (!args.preventevent)
                plot.getplaceholder().trigger("plotzoom", [ plot ]);
        }

        plot.pan = function (args) {
            var delta = {
                x: +args.left,
                y: +args.top
            };

            if (isnan(delta.x))
                delta.x = 0;
            if (isnan(delta.y))
                delta.y = 0;

            $.each(plot.getaxes(), function (_, axis) {
                var opts = axis.options,
                    min, max, d = delta[axis.direction];

                min = axis.c2p(axis.p2c(axis.min) + d),
                max = axis.c2p(axis.p2c(axis.max) + d);

                var pr = opts.panrange;
                if (pr === false) // no panning on this axis
                    return;
                
                if (pr) {
                    // check whether we hit the wall
                    if (pr[0] != null && pr[0] > min) {
                        d = pr[0] - min;
                        min += d;
                        max += d;
                    }
                    
                    if (pr[1] != null && pr[1] < max) {
                        d = pr[1] - max;
                        min += d;
                        max += d;
                    }
                }
                
                opts.min = min;
                opts.max = max;
            });
            
            plot.setupgrid();
            plot.draw();
            
            if (!args.preventevent)
                plot.getplaceholder().trigger("plotpan", [ plot ]);
        }

        function shutdown(plot, eventholder) {
            eventholder.unbind(plot.getoptions().zoom.trigger, onzoomclick);
            eventholder.unbind("mousewheel", onmousewheel);
            eventholder.unbind("dragstart", ondragstart);
            eventholder.unbind("drag", ondrag);
            eventholder.unbind("dragend", ondragend);
            if (pantimeout)
                cleartimeout(pantimeout);
        }
        
        plot.hooks.bindevents.push(bindevents);
        plot.hooks.shutdown.push(shutdown);
    }
    
    $.plot.plugins.push({
        init: init,
        options: options,
        name: 'navigate',
        version: '1.3'
    });
})(jquery);
