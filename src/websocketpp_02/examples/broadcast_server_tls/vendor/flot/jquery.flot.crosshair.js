/*
flot plugin for showing crosshairs, thin lines, when the mouse hovers
over the plot.

  crosshair: {
    mode: null or "x" or "y" or "xy"
    color: color
    linewidth: number
  }

set the mode to one of "x", "y" or "xy". the "x" mode enables a
vertical crosshair that lets you trace the values on the x axis, "y"
enables a horizontal crosshair and "xy" enables them both. "color" is
the color of the crosshair (default is "rgba(170, 0, 0, 0.80)"),
"linewidth" is the width of the drawn lines (default is 1).

the plugin also adds four public methods:

  - setcrosshair(pos)

    set the position of the crosshair. note that this is cleared if
    the user moves the mouse. "pos" is in coordinates of the plot and
    should be on the form { x: xpos, y: ypos } (you can use x2/x3/...
    if you're using multiple axes), which is coincidentally the same
    format as what you get from a "plothover" event. if "pos" is null,
    the crosshair is cleared.

  - clearcrosshair()

    clear the crosshair.

  - lockcrosshair(pos)

    cause the crosshair to lock to the current location, no longer
    updating if the user moves the mouse. optionally supply a position
    (passed on to setcrosshair()) to move it to.

    example usage:
      var myflot = $.plot( $("#graph"), ..., { crosshair: { mode: "x" } } };
      $("#graph").bind("plothover", function (evt, position, item) {
        if (item) {
          // lock the crosshair to the data point being hovered
          myflot.lockcrosshair({ x: item.datapoint[0], y: item.datapoint[1] });
        }
        else {
          // return normal crosshair operation
          myflot.unlockcrosshair();
        }
      });

  - unlockcrosshair()

    free the crosshair to move again after locking it.
*/

(function ($) {
    var options = {
        crosshair: {
            mode: null, // one of null, "x", "y" or "xy",
            color: "rgba(170, 0, 0, 0.80)",
            linewidth: 1
        }
    };
    
    function init(plot) {
        // position of crosshair in pixels
        var crosshair = { x: -1, y: -1, locked: false };

        plot.setcrosshair = function setcrosshair(pos) {
            if (!pos)
                crosshair.x = -1;
            else {
                var o = plot.p2c(pos);
                crosshair.x = math.max(0, math.min(o.left, plot.width()));
                crosshair.y = math.max(0, math.min(o.top, plot.height()));
            }
            
            plot.triggerredrawoverlay();
        };
        
        plot.clearcrosshair = plot.setcrosshair; // passes null for pos
        
        plot.lockcrosshair = function lockcrosshair(pos) {
            if (pos)
                plot.setcrosshair(pos);
            crosshair.locked = true;
        }

        plot.unlockcrosshair = function unlockcrosshair() {
            crosshair.locked = false;
        }

        function onmouseout(e) {
            if (crosshair.locked)
                return;

            if (crosshair.x != -1) {
                crosshair.x = -1;
                plot.triggerredrawoverlay();
            }
        }

        function onmousemove(e) {
            if (crosshair.locked)
                return;
                
            if (plot.getselection && plot.getselection()) {
                crosshair.x = -1; // hide the crosshair while selecting
                return;
            }
                
            var offset = plot.offset();
            crosshair.x = math.max(0, math.min(e.pagex - offset.left, plot.width()));
            crosshair.y = math.max(0, math.min(e.pagey - offset.top, plot.height()));
            plot.triggerredrawoverlay();
        }
        
        plot.hooks.bindevents.push(function (plot, eventholder) {
            if (!plot.getoptions().crosshair.mode)
                return;

            eventholder.mouseout(onmouseout);
            eventholder.mousemove(onmousemove);
        });

        plot.hooks.drawoverlay.push(function (plot, ctx) {
            var c = plot.getoptions().crosshair;
            if (!c.mode)
                return;

            var plotoffset = plot.getplotoffset();
            
            ctx.save();
            ctx.translate(plotoffset.left, plotoffset.top);

            if (crosshair.x != -1) {
                ctx.strokestyle = c.color;
                ctx.linewidth = c.linewidth;
                ctx.linejoin = "round";

                ctx.beginpath();
                if (c.mode.indexof("x") != -1) {
                    ctx.moveto(crosshair.x, 0);
                    ctx.lineto(crosshair.x, plot.height());
                }
                if (c.mode.indexof("y") != -1) {
                    ctx.moveto(0, crosshair.y);
                    ctx.lineto(plot.width(), crosshair.y);
                }
                ctx.stroke();
            }
            ctx.restore();
        });

        plot.hooks.shutdown.push(function (plot, eventholder) {
            eventholder.unbind("mouseout", onmouseout);
            eventholder.unbind("mousemove", onmousemove);
        });
    }
    
    $.plot.plugins.push({
        init: init,
        options: options,
        name: 'crosshair',
        version: '1.0'
    });
})(jquery);
