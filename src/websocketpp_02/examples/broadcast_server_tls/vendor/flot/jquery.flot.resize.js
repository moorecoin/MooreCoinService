/*
flot plugin for automatically redrawing plots when the placeholder
size changes, e.g. on window resizes.

it works by listening for changes on the placeholder div (through the
jquery resize event plugin) - if the size changes, it will redraw the
plot.

there are no options. if you need to disable the plugin for some
plots, you can just fix the size of their placeholders.
*/


/* inline dependency: 
 * jquery resize event - v1.1 - 3/14/2010
 * http://benalman.com/projects/jquery-resize-plugin/
 * 
 * copyright (c) 2010 "cowboy" ben alman
 * dual licensed under the mit and gpl licenses.
 * http://benalman.com/about/license/
 */
(function($,h,c){var a=$([]),e=$.resize=$.extend($.resize,{}),i,k="settimeout",j="resize",d=j+"-special-event",b="delay",f="throttlewindow";e[b]=250;e[f]=true;$.event.special[j]={setup:function(){if(!e[f]&&this[k]){return false}var l=$(this);a=a.add(l);$.data(this,d,{w:l.width(),h:l.height()});if(a.length===1){g()}},teardown:function(){if(!e[f]&&this[k]){return false}var l=$(this);a=a.not(l);l.removedata(d);if(!a.length){cleartimeout(i)}},add:function(l){if(!e[f]&&this[k]){return false}var n;function m(s,o,p){var q=$(this),r=$.data(this,d);r.w=o!==c?o:q.width();r.h=p!==c?p:q.height();n.apply(this,arguments)}if($.isfunction(l)){n=l;return m}else{n=l.handler;l.handler=m}}};function g(){i=h[k](function(){a.each(function(){var n=$(this),m=n.width(),l=n.height(),o=$.data(this,d);if(m!==o.w||l!==o.h){n.trigger(j,[o.w=m,o.h=l])}});g()},e[b])}})(jquery,this);


(function ($) {
    var options = { }; // no options

    function init(plot) {
        function onresize() {
            var placeholder = plot.getplaceholder();

            // somebody might have hidden us and we can't plot
            // when we don't have the dimensions
            if (placeholder.width() == 0 || placeholder.height() == 0)
                return;

            plot.resize();
            plot.setupgrid();
            plot.draw();
        }
        
        function bindevents(plot, eventholder) {
            plot.getplaceholder().resize(onresize);
        }

        function shutdown(plot, eventholder) {
            plot.getplaceholder().unbind("resize", onresize);
        }
        
        plot.hooks.bindevents.push(bindevents);
        plot.hooks.shutdown.push(shutdown);
    }
    
    $.plot.plugins.push({
        init: init,
        options: options,
        name: 'resize',
        version: '1.0'
    });
})(jquery);
