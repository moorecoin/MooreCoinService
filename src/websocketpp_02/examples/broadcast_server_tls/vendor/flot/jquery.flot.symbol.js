/*
flot plugin that adds some extra symbols for plotting points.

the symbols are accessed as strings through the standard symbol
choice:

  series: {
      points: {
          symbol: "square" // or "diamond", "triangle", "cross"
      }
  }

*/

(function ($) {
    function processrawdata(plot, series, datapoints) {
        // we normalize the area of each symbol so it is approximately the
        // same as a circle of the given radius

        var handlers = {
            square: function (ctx, x, y, radius, shadow) {
                // pi * r^2 = (2s)^2  =>  s = r * sqrt(pi)/2
                var size = radius * math.sqrt(math.pi) / 2;
                ctx.rect(x - size, y - size, size + size, size + size);
            },
            diamond: function (ctx, x, y, radius, shadow) {
                // pi * r^2 = 2s^2  =>  s = r * sqrt(pi/2)
                var size = radius * math.sqrt(math.pi / 2);
                ctx.moveto(x - size, y);
                ctx.lineto(x, y - size);
                ctx.lineto(x + size, y);
                ctx.lineto(x, y + size);
                ctx.lineto(x - size, y);
            },
            triangle: function (ctx, x, y, radius, shadow) {
                // pi * r^2 = 1/2 * s^2 * sin (pi / 3)  =>  s = r * sqrt(2 * pi / sin(pi / 3))
                var size = radius * math.sqrt(2 * math.pi / math.sin(math.pi / 3));
                var height = size * math.sin(math.pi / 3);
                ctx.moveto(x - size/2, y + height/2);
                ctx.lineto(x + size/2, y + height/2);
                if (!shadow) {
                    ctx.lineto(x, y - height/2);
                    ctx.lineto(x - size/2, y + height/2);
                }
            },
            cross: function (ctx, x, y, radius, shadow) {
                // pi * r^2 = (2s)^2  =>  s = r * sqrt(pi)/2
                var size = radius * math.sqrt(math.pi) / 2;
                ctx.moveto(x - size, y - size);
                ctx.lineto(x + size, y + size);
                ctx.moveto(x - size, y + size);
                ctx.lineto(x + size, y - size);
            }
        }

        var s = series.points.symbol;
        if (handlers[s])
            series.points.symbol = handlers[s];
    }
    
    function init(plot) {
        plot.hooks.processdatapoints.push(processrawdata);
    }
    
    $.plot.plugins.push({
        init: init,
        name: 'symbols',
        version: '1.0'
    });
})(jquery);
