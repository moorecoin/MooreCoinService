/*
flot plugin for selecting regions.

the plugin defines the following options:

  selection: {
    mode: null or "x" or "y" or "xy",
    color: color
  }

selection support is enabled by setting the mode to one of "x", "y" or
"xy". in "x" mode, the user will only be able to specify the x range,
similarly for "y" mode. for "xy", the selection becomes a rectangle
where both ranges can be specified. "color" is color of the selection
(if you need to change the color later on, you can get to it with
plot.getoptions().selection.color).

when selection support is enabled, a "plotselected" event will be
emitted on the dom element you passed into the plot function. the
event handler gets a parameter with the ranges selected on the axes,
like this:

  placeholder.bind("plotselected", function(event, ranges) {
    alert("you selected " + ranges.xaxis.from + " to " + ranges.xaxis.to)
    // similar for yaxis - with multiple axes, the extra ones are in
    // x2axis, x3axis, ...
  });

the "plotselected" event is only fired when the user has finished
making the selection. a "plotselecting" event is fired during the
process with the same parameters as the "plotselected" event, in case
you want to know what's happening while it's happening,

a "plotunselected" event with no arguments is emitted when the user
clicks the mouse to remove the selection.

the plugin allso adds the following methods to the plot object:

- setselection(ranges, preventevent)

  set the selection rectangle. the passed in ranges is on the same
  form as returned in the "plotselected" event. if the selection mode
  is "x", you should put in either an xaxis range, if the mode is "y"
  you need to put in an yaxis range and both xaxis and yaxis if the
  selection mode is "xy", like this:

    setselection({ xaxis: { from: 0, to: 10 }, yaxis: { from: 40, to: 60 } });

  setselection will trigger the "plotselected" event when called. if
  you don't want that to happen, e.g. if you're inside a
  "plotselected" handler, pass true as the second parameter. if you
  are using multiple axes, you can specify the ranges on any of those,
  e.g. as x2axis/x3axis/... instead of xaxis, the plugin picks the
  first one it sees.
  
- clearselection(preventevent)

  clear the selection rectangle. pass in true to avoid getting a
  "plotunselected" event.

- getselection()

  returns the current selection in the same format as the
  "plotselected" event. if there's currently no selection, the
  function returns null.

*/

(function ($) {
    function init(plot) {
        var selection = {
                first: { x: -1, y: -1}, second: { x: -1, y: -1},
                show: false,
                active: false
            };

        // fixme: the drag handling implemented here should be
        // abstracted out, there's some similar code from a library in
        // the navigation plugin, this should be massaged a bit to fit
        // the flot cases here better and reused. doing this would
        // make this plugin much slimmer.
        var savedhandlers = {};

        var mouseuphandler = null;
        
        function onmousemove(e) {
            if (selection.active) {
                updateselection(e);
                
                plot.getplaceholder().trigger("plotselecting", [ getselection() ]);
            }
        }

        function onmousedown(e) {
            if (e.which != 1)  // only accept left-click
                return;
            
            // cancel out any text selections
            document.body.focus();

            // prevent text selection and drag in old-school browsers
            if (document.onselectstart !== undefined && savedhandlers.onselectstart == null) {
                savedhandlers.onselectstart = document.onselectstart;
                document.onselectstart = function () { return false; };
            }
            if (document.ondrag !== undefined && savedhandlers.ondrag == null) {
                savedhandlers.ondrag = document.ondrag;
                document.ondrag = function () { return false; };
            }

            setselectionpos(selection.first, e);

            selection.active = true;

            // this is a bit silly, but we have to use a closure to be
            // able to whack the same handler again
            mouseuphandler = function (e) { onmouseup(e); };
            
            $(document).one("mouseup", mouseuphandler);
        }

        function onmouseup(e) {
            mouseuphandler = null;
            
            // revert drag stuff for old-school browsers
            if (document.onselectstart !== undefined)
                document.onselectstart = savedhandlers.onselectstart;
            if (document.ondrag !== undefined)
                document.ondrag = savedhandlers.ondrag;

            // no more dragging
            selection.active = false;
            updateselection(e);

            if (selectionissane())
                triggerselectedevent();
            else {
                // this counts as a clear
                plot.getplaceholder().trigger("plotunselected", [ ]);
                plot.getplaceholder().trigger("plotselecting", [ null ]);
            }

            return false;
        }

        function getselection() {
            if (!selectionissane())
                return null;

            var r = {}, c1 = selection.first, c2 = selection.second;
            $.each(plot.getaxes(), function (name, axis) {
                if (axis.used) {
                    var p1 = axis.c2p(c1[axis.direction]), p2 = axis.c2p(c2[axis.direction]); 
                    r[name] = { from: math.min(p1, p2), to: math.max(p1, p2) };
                }
            });
            return r;
        }

        function triggerselectedevent() {
            var r = getselection();

            plot.getplaceholder().trigger("plotselected", [ r ]);

            // backwards-compat stuff, to be removed in future
            if (r.xaxis && r.yaxis)
                plot.getplaceholder().trigger("selected", [ { x1: r.xaxis.from, y1: r.yaxis.from, x2: r.xaxis.to, y2: r.yaxis.to } ]);
        }

        function clamp(min, value, max) {
            return value < min ? min: (value > max ? max: value);
        }

        function setselectionpos(pos, e) {
            var o = plot.getoptions();
            var offset = plot.getplaceholder().offset();
            var plotoffset = plot.getplotoffset();
            pos.x = clamp(0, e.pagex - offset.left - plotoffset.left, plot.width());
            pos.y = clamp(0, e.pagey - offset.top - plotoffset.top, plot.height());

            if (o.selection.mode == "y")
                pos.x = pos == selection.first ? 0 : plot.width();

            if (o.selection.mode == "x")
                pos.y = pos == selection.first ? 0 : plot.height();
        }

        function updateselection(pos) {
            if (pos.pagex == null)
                return;

            setselectionpos(selection.second, pos);
            if (selectionissane()) {
                selection.show = true;
                plot.triggerredrawoverlay();
            }
            else
                clearselection(true);
        }

        function clearselection(preventevent) {
            if (selection.show) {
                selection.show = false;
                plot.triggerredrawoverlay();
                if (!preventevent)
                    plot.getplaceholder().trigger("plotunselected", [ ]);
            }
        }

        // function taken from markings support in flot
        function extractrange(ranges, coord) {
            var axis, from, to, key, axes = plot.getaxes();

            for (var k in axes) {
                axis = axes[k];
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
                axis = coord == "x" ? plot.getxaxes()[0] : plot.getyaxes()[0];
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
        
        function setselection(ranges, preventevent) {
            var axis, range, o = plot.getoptions();

            if (o.selection.mode == "y") {
                selection.first.x = 0;
                selection.second.x = plot.width();
            }
            else {
                range = extractrange(ranges, "x");

                selection.first.x = range.axis.p2c(range.from);
                selection.second.x = range.axis.p2c(range.to);
            }

            if (o.selection.mode == "x") {
                selection.first.y = 0;
                selection.second.y = plot.height();
            }
            else {
                range = extractrange(ranges, "y");

                selection.first.y = range.axis.p2c(range.from);
                selection.second.y = range.axis.p2c(range.to);
            }

            selection.show = true;
            plot.triggerredrawoverlay();
            if (!preventevent && selectionissane())
                triggerselectedevent();
        }

        function selectionissane() {
            var minsize = 5;
            return math.abs(selection.second.x - selection.first.x) >= minsize &&
                math.abs(selection.second.y - selection.first.y) >= minsize;
        }

        plot.clearselection = clearselection;
        plot.setselection = setselection;
        plot.getselection = getselection;

        plot.hooks.bindevents.push(function(plot, eventholder) {
            var o = plot.getoptions();
            if (o.selection.mode != null) {
                eventholder.mousemove(onmousemove);
                eventholder.mousedown(onmousedown);
            }
        });


        plot.hooks.drawoverlay.push(function (plot, ctx) {
            // draw selection
            if (selection.show && selectionissane()) {
                var plotoffset = plot.getplotoffset();
                var o = plot.getoptions();

                ctx.save();
                ctx.translate(plotoffset.left, plotoffset.top);

                var c = $.color.parse(o.selection.color);

                ctx.strokestyle = c.scale('a', 0.8).tostring();
                ctx.linewidth = 1;
                ctx.linejoin = "round";
                ctx.fillstyle = c.scale('a', 0.4).tostring();

                var x = math.min(selection.first.x, selection.second.x),
                    y = math.min(selection.first.y, selection.second.y),
                    w = math.abs(selection.second.x - selection.first.x),
                    h = math.abs(selection.second.y - selection.first.y);

                ctx.fillrect(x, y, w, h);
                ctx.strokerect(x, y, w, h);

                ctx.restore();
            }
        });
        
        plot.hooks.shutdown.push(function (plot, eventholder) {
            eventholder.unbind("mousemove", onmousemove);
            eventholder.unbind("mousedown", onmousedown);
            
            if (mouseuphandler)
                $(document).unbind("mouseup", mouseuphandler);
        });

    }

    $.plot.plugins.push({
        init: init,
        options: {
            selection: {
                mode: null, // one of null, "x", "y" or "xy"
                color: "#e8cfac"
            }
        },
        name: 'selection',
        version: '1.1'
    });
})(jquery);
