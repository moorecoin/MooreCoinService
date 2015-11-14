/*
flot plugin for thresholding data. controlled through the option
"threshold" in either the global series options

  series: {
    threshold: {
      below: number
      color: colorspec
    }
  }

or in a specific series

  $.plot($("#placeholder"), [{ data: [ ... ], threshold: { ... }}])

the data points below "below" are drawn with the specified color. this
makes it easy to mark points below 0, e.g. for budget data.

internally, the plugin works by splitting the data into two series,
above and below the threshold. the extra series below the threshold
will have its label cleared and the special "originseries" attribute
set to the original series. you may need to check for this in hover
events.
*/

(function ($) {
    var options = {
        series: { threshold: null } // or { below: number, color: color spec}
    };
    
    function init(plot) {
        function thresholddata(plot, s, datapoints) {
            if (!s.threshold)
                return;
            
            var ps = datapoints.pointsize, i, x, y, p, prevp,
                thresholded = $.extend({}, s); // note: shallow copy

            thresholded.datapoints = { points: [], pointsize: ps };
            thresholded.label = null;
            thresholded.color = s.threshold.color;
            thresholded.threshold = null;
            thresholded.originseries = s;
            thresholded.data = [];

            var below = s.threshold.below,
                origpoints = datapoints.points,
                addcrossingpoints = s.lines.show;

            threspoints = [];
            newpoints = [];

            for (i = 0; i < origpoints.length; i += ps) {
                x = origpoints[i]
                y = origpoints[i + 1];

                prevp = p;
                if (y < below)
                    p = threspoints;
                else
                    p = newpoints;

                if (addcrossingpoints && prevp != p && x != null
                    && i > 0 && origpoints[i - ps] != null) {
                    var interx = (x - origpoints[i - ps]) / (y - origpoints[i - ps + 1]) * (below - y) + x;
                    prevp.push(interx);
                    prevp.push(below);
                    for (m = 2; m < ps; ++m)
                        prevp.push(origpoints[i + m]);
                    
                    p.push(null); // start new segment
                    p.push(null);
                    for (m = 2; m < ps; ++m)
                        p.push(origpoints[i + m]);
                    p.push(interx);
                    p.push(below);
                    for (m = 2; m < ps; ++m)
                        p.push(origpoints[i + m]);
                }

                p.push(x);
                p.push(y);
            }

            datapoints.points = newpoints;
            thresholded.datapoints.points = threspoints;
            
            if (thresholded.datapoints.points.length > 0)
                plot.getdata().push(thresholded);
                
            // fixme: there are probably some edge cases left in bars
        }
        
        plot.hooks.processdatapoints.push(thresholddata);
    }
    
    $.plot.plugins.push({
        init: init,
        options: options,
        name: 'threshold',
        version: '1.0'
    });
})(jquery);
