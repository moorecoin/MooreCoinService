about
-----

flot is a javascript plotting library for jquery. read more at the
website:

  http://code.google.com/p/flot/

take a look at the examples linked from above, they should give a good
impression of what flot can do and the source code of the examples is
probably the fastest way to learn how to use flot.
  

installation
------------

just include the javascript file after you've included jquery.

generally, all browsers that support the html5 canvas tag are
supported.

for support for internet explorer < 9, you can use excanvas, a canvas
emulator; this is used in the examples bundled with flot. you just
include the excanvas script like this:

  <!--[if lte ie 8]><script language="javascript" type="text/javascript" src="excanvas.min.js"></script><![endif]-->

if it's not working on your development ie 6.0, check that it has
support for vml which excanvas is relying on. it appears that some
stripped down versions used for test environments on virtual machines
lack the vml support.

you can also try using flashcanvas (see
http://code.google.com/p/flashcanvas/), which uses flash to do the
emulation. although flash can be a bit slower to load than vml, if
you've got a lot of points, the flash version can be much faster
overall. flot contains some wrapper code for activating excanvas which
flashcanvas is compatible with.

you need at least jquery 1.2.6, but try at least 1.3.2 for interactive
charts because of performance improvements in event handling.


basic usage
-----------

create a placeholder div to put the graph in:

   <div id="placeholder"></div>

you need to set the width and height of this div, otherwise the plot
library doesn't know how to scale the graph. you can do it inline like
this:

   <div id="placeholder" style="width:600px;height:300px"></div>

you can also do it with an external stylesheet. make sure that the
placeholder isn't within something with a display:none css property -
in that case, flot has trouble measuring label dimensions which
results in garbled looks and might have trouble measuring the
placeholder dimensions which is fatal (it'll throw an exception).

then when the div is ready in the dom, which is usually on document
ready, run the plot function:

  $.plot($("#placeholder"), data, options);

here, data is an array of data series and options is an object with
settings if you want to customize the plot. take a look at the
examples for some ideas of what to put in or look at the reference
in the file "api.txt". here's a quick example that'll draw a line from
(0, 0) to (1, 1):

  $.plot($("#placeholder"), [ [[0, 0], [1, 1]] ], { yaxis: { max: 1 } });

the plot function immediately draws the chart and then returns a plot
object with a couple of methods.


what's with the name?
---------------------

first: it's pronounced with a short o, like "plot". not like "flawed".

so "flot" rhymes with "plot".

and if you look up "flot" in a danish-to-english dictionary, some up
the words that come up are "good-looking", "attractive", "stylish",
"smart", "impressive", "extravagant". one of the main goals with flot
is pretty looks.
