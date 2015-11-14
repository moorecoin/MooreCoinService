/*
flot plugin for rendering pie charts. the plugin assumes the data is 
coming is as a single data value for each series, and each of those 
values is a positive value or zero (negative numbers don't make 
any sense and will cause strange effects). the data values do 
not need to be passed in as percentage values because it 
internally calculates the total and percentages.

* created by brian medendorp, june 2009
* updated november 2009 with contributions from: btburnett3, anthony aragues and xavi ivars

* changes:
	2009-10-22: linejoin set to round
	2009-10-23: ie full circle fix, donut
	2009-11-11: added basic hover from btburnett3 - does not work in ie, and center is off in chrome and opera
	2009-11-17: added ie hover capability submitted by anthony aragues
	2009-11-18: added bug fix submitted by xavi ivars (issues with arrays when other js libraries are included as well)
		

available options are:
series: {
	pie: {
		show: true/false
		radius: 0-1 for percentage of fullsize, or a specified pixel length, or 'auto'
		innerradius: 0-1 for percentage of fullsize or a specified pixel length, for creating a donut effect
		startangle: 0-2 factor of pi used for starting angle (in radians) i.e 3/2 starts at the top, 0 and 2 have the same result
		tilt: 0-1 for percentage to tilt the pie, where 1 is no tilt, and 0 is completely flat (nothing will show)
		offset: {
			top: integer value to move the pie up or down
			left: integer value to move the pie left or right, or 'auto'
		},
		stroke: {
			color: any hexidecimal color value (other formats may or may not work, so best to stick with something like '#fff')
			width: integer pixel width of the stroke
		},
		label: {
			show: true/false, or 'auto'
			formatter:  a user-defined function that modifies the text/style of the label text
			radius: 0-1 for percentage of fullsize, or a specified pixel length
			background: {
				color: any hexidecimal color value (other formats may or may not work, so best to stick with something like '#000')
				opacity: 0-1
			},
			threshold: 0-1 for the percentage value at which to hide labels (if they're too small)
		},
		combine: {
			threshold: 0-1 for the percentage value at which to combine slices (if they're too small)
			color: any hexidecimal color value (other formats may or may not work, so best to stick with something like '#ccc'), if null, the plugin will automatically use the color of the first slice to be combined
			label: any text value of what the combined slice should be labeled
		}
		highlight: {
			opacity: 0-1
		}
	}
}

more detail and specific examples can be found in the included html file.

*/

(function ($) 
{
	function init(plot) // this is the "body" of the plugin
	{
		var canvas = null;
		var target = null;
		var maxradius = null;
		var centerleft = null;
		var centertop = null;
		var total = 0;
		var redraw = true;
		var redrawattempts = 10;
		var shrink = 0.95;
		var legendwidth = 0;
		var processed = false;
		var raw = false;
		
		// interactive variables	
		var highlights = [];	
	
		// add hook to determine if pie plugin in enabled, and then perform necessary operations
		plot.hooks.processoptions.push(checkpieenabled);
		plot.hooks.bindevents.push(bindevents);	

		// check to see if the pie plugin is enabled
		function checkpieenabled(plot, options)
		{
			if (options.series.pie.show)
			{
				//disable grid
				options.grid.show = false;
				
				// set labels.show
				if (options.series.pie.label.show=='auto')
					if (options.legend.show)
						options.series.pie.label.show = false;
					else
						options.series.pie.label.show = true;
				
				// set radius
				if (options.series.pie.radius=='auto')
					if (options.series.pie.label.show)
						options.series.pie.radius = 3/4;
					else
						options.series.pie.radius = 1;
						
				// ensure sane tilt
				if (options.series.pie.tilt>1)
					options.series.pie.tilt=1;
				if (options.series.pie.tilt<0)
					options.series.pie.tilt=0;
			
				// add processdata hook to do transformations on the data
				plot.hooks.processdatapoints.push(processdatapoints);
				plot.hooks.drawoverlay.push(drawoverlay);	
				
				// add draw hook
				plot.hooks.draw.push(draw);
			}
		}
	
		// bind hoverable events
		function bindevents(plot, eventholder) 		
		{		
			var options = plot.getoptions();
			
			if (options.series.pie.show && options.grid.hoverable)
				eventholder.unbind('mousemove').mousemove(onmousemove);
				
			if (options.series.pie.show && options.grid.clickable)
				eventholder.unbind('click').click(onclick);
		}	
		

		// debugging function that prints out an object
		function alertobject(obj)
		{
			var msg = '';
			function traverse(obj, depth)
			{
				if (!depth)
					depth = 0;
				for (var i = 0; i < obj.length; ++i)
				{
					for (var j=0; j<depth; j++)
						msg += '\t';
				
					if( typeof obj[i] == "object")
					{	// its an object
						msg += ''+i+':\n';
						traverse(obj[i], depth+1);
					}
					else
					{	// its a value
						msg += ''+i+': '+obj[i]+'\n';
					}
				}
			}
			traverse(obj);
			alert(msg);
		}
		
		function calctotal(data)
		{
			for (var i = 0; i < data.length; ++i)
			{
				var item = parsefloat(data[i].data[0][1]);
				if (item)
					total += item;
			}
		}	
		
		function processdatapoints(plot, series, data, datapoints) 
		{	
			if (!processed)
			{
				processed = true;
			
				canvas = plot.getcanvas();
				target = $(canvas).parent();
				options = plot.getoptions();
			
				plot.setdata(combine(plot.getdata()));
			}
		}
		
		function setuppie()
		{
			legendwidth = target.children().filter('.legend').children().width();
		
			// calculate maximum radius and center point
			maxradius =  math.min(canvas.width,(canvas.height/options.series.pie.tilt))/2;
			centertop = (canvas.height/2)+options.series.pie.offset.top;
			centerleft = (canvas.width/2);
			
			if (options.series.pie.offset.left=='auto')
				if (options.legend.position.match('w'))
					centerleft += legendwidth/2;
				else
					centerleft -= legendwidth/2;
			else
				centerleft += options.series.pie.offset.left;
					
			if (centerleft<maxradius)
				centerleft = maxradius;
			else if (centerleft>canvas.width-maxradius)
				centerleft = canvas.width-maxradius;
		}
		
		function fixdata(data)
		{
			for (var i = 0; i < data.length; ++i)
			{
				if (typeof(data[i].data)=='number')
					data[i].data = [[1,data[i].data]];
				else if (typeof(data[i].data)=='undefined' || typeof(data[i].data[0])=='undefined')
				{
					if (typeof(data[i].data)!='undefined' && typeof(data[i].data.label)!='undefined')
						data[i].label = data[i].data.label; // fix weirdness coming from flot
					data[i].data = [[1,0]];
					
				}
			}
			return data;
		}
		
		function combine(data)
		{
			data = fixdata(data);
			calctotal(data);
			var combined = 0;
			var numcombined = 0;
			var color = options.series.pie.combine.color;
			
			var newdata = [];
			for (var i = 0; i < data.length; ++i)
			{
				// make sure its a number
				data[i].data[0][1] = parsefloat(data[i].data[0][1]);
				if (!data[i].data[0][1])
					data[i].data[0][1] = 0;
					
				if (data[i].data[0][1]/total<=options.series.pie.combine.threshold)
				{
					combined += data[i].data[0][1];
					numcombined++;
					if (!color)
						color = data[i].color;
				}				
				else
				{
					newdata.push({
						data: [[1,data[i].data[0][1]]], 
						color: data[i].color, 
						label: data[i].label,
						angle: (data[i].data[0][1]*(math.pi*2))/total,
						percent: (data[i].data[0][1]/total*100)
					});
				}
			}
			if (numcombined>0)
				newdata.push({
					data: [[1,combined]], 
					color: color, 
					label: options.series.pie.combine.label,
					angle: (combined*(math.pi*2))/total,
					percent: (combined/total*100)
				});
			return newdata;
		}		
		
		function draw(plot, newctx)
		{
			if (!target) return; // if no series were passed
			ctx = newctx;
		
			setuppie();
			var slices = plot.getdata();
		
			var attempts = 0;
			while (redraw && attempts<redrawattempts)
			{
				redraw = false;
				if (attempts>0)
					maxradius *= shrink;
				attempts += 1;
				clear();
				if (options.series.pie.tilt<=0.8)
					drawshadow();
				drawpie();
			}
			if (attempts >= redrawattempts) {
				clear();
				target.prepend('<div class="error">could not draw pie with labels contained inside canvas</div>');
			}
			
			if ( plot.setseries && plot.insertlegend )
			{
				plot.setseries(slices);
				plot.insertlegend();
			}
			
			// we're actually done at this point, just defining internal functions at this point
			
			function clear()
			{
				ctx.clearrect(0,0,canvas.width,canvas.height);
				target.children().filter('.pielabel, .pielabelbackground').remove();
			}
			
			function drawshadow()
			{
				var shadowleft = 5;
				var shadowtop = 15;
				var edge = 10;
				var alpha = 0.02;
			
				// set radius
				if (options.series.pie.radius>1)
					var radius = options.series.pie.radius;
				else
					var radius = maxradius * options.series.pie.radius;
					
				if (radius>=(canvas.width/2)-shadowleft || radius*options.series.pie.tilt>=(canvas.height/2)-shadowtop || radius<=edge)
					return;	// shadow would be outside canvas, so don't draw it
			
				ctx.save();
				ctx.translate(shadowleft,shadowtop);
				ctx.globalalpha = alpha;
				ctx.fillstyle = '#000';

				// center and rotate to starting position
				ctx.translate(centerleft,centertop);
				ctx.scale(1, options.series.pie.tilt);
				
				//radius -= edge;
				for (var i=1; i<=edge; i++)
				{
					ctx.beginpath();
					ctx.arc(0,0,radius,0,math.pi*2,false);
					ctx.fill();
					radius -= i;
				}	
				
				ctx.restore();
			}
			
			function drawpie()
			{
				startangle = math.pi*options.series.pie.startangle;
				
				// set radius
				if (options.series.pie.radius>1)
					var radius = options.series.pie.radius;
				else
					var radius = maxradius * options.series.pie.radius;
				
				// center and rotate to starting position
				ctx.save();
				ctx.translate(centerleft,centertop);
				ctx.scale(1, options.series.pie.tilt);
				//ctx.rotate(startangle); // start at top; -- this doesn't work properly in opera
				
				// draw slices
				ctx.save();
				var currentangle = startangle;
				for (var i = 0; i < slices.length; ++i)
				{
					slices[i].startangle = currentangle;
					drawslice(slices[i].angle, slices[i].color, true);
				}
				ctx.restore();
				
				// draw slice outlines
				ctx.save();
				ctx.linewidth = options.series.pie.stroke.width;
				currentangle = startangle;
				for (var i = 0; i < slices.length; ++i)
					drawslice(slices[i].angle, options.series.pie.stroke.color, false);
				ctx.restore();
					
				// draw donut hole
				drawdonuthole(ctx);
				
				// draw labels
				if (options.series.pie.label.show)
					drawlabels();
				
				// restore to original state
				ctx.restore();
				
				function drawslice(angle, color, fill)
				{	
					if (angle<=0)
						return;
				
					if (fill)
						ctx.fillstyle = color;
					else
					{
						ctx.strokestyle = color;
						ctx.linejoin = 'round';
					}
						
					ctx.beginpath();
					if (math.abs(angle - math.pi*2) > 0.000000001)
						ctx.moveto(0,0); // center of the pie
					else if ($.browser.msie)
						angle -= 0.0001;
					//ctx.arc(0,0,radius,0,angle,false); // this doesn't work properly in opera
					ctx.arc(0,0,radius,currentangle,currentangle+angle,false);
					ctx.closepath();
					//ctx.rotate(angle); // this doesn't work properly in opera
					currentangle += angle;
					
					if (fill)
						ctx.fill();
					else
						ctx.stroke();
				}
				
				function drawlabels()
				{
					var currentangle = startangle;
					
					// set radius
					if (options.series.pie.label.radius>1)
						var radius = options.series.pie.label.radius;
					else
						var radius = maxradius * options.series.pie.label.radius;
					
					for (var i = 0; i < slices.length; ++i)
					{
						if (slices[i].percent >= options.series.pie.label.threshold*100)
							drawlabel(slices[i], currentangle, i);
						currentangle += slices[i].angle;
					}
					
					function drawlabel(slice, startangle, index)
					{
						if (slice.data[0][1]==0)
							return;
							
						// format label text
						var lf = options.legend.labelformatter, text, plf = options.series.pie.label.formatter;
						if (lf)
							text = lf(slice.label, slice);
						else
							text = slice.label;
						if (plf)
							text = plf(text, slice);
							
						var halfangle = ((startangle+slice.angle) + startangle)/2;
						var x = centerleft + math.round(math.cos(halfangle) * radius);
						var y = centertop + math.round(math.sin(halfangle) * radius) * options.series.pie.tilt;
						
						var html = '<span class="pielabel" id="pielabel'+index+'" style="position:absolute;top:' + y + 'px;left:' + x + 'px;">' + text + "</span>";
						target.append(html);
						var label = target.children('#pielabel'+index);
						var labeltop = (y - label.height()/2);
						var labelleft = (x - label.width()/2);
						label.css('top', labeltop);
						label.css('left', labelleft);
						
						// check to make sure that the label is not outside the canvas
						if (0-labeltop>0 || 0-labelleft>0 || canvas.height-(labeltop+label.height())<0 || canvas.width-(labelleft+label.width())<0)
							redraw = true;
						
						if (options.series.pie.label.background.opacity != 0) {
							// put in the transparent background separately to avoid blended labels and label boxes
							var c = options.series.pie.label.background.color;
							if (c == null) {
								c = slice.color;
							}
							var pos = 'top:'+labeltop+'px;left:'+labelleft+'px;';
							$('<div class="pielabelbackground" style="position:absolute;width:' + label.width() + 'px;height:' + label.height() + 'px;' + pos +'background-color:' + c + ';"> </div>').insertbefore(label).css('opacity', options.series.pie.label.background.opacity);
						}
					} // end individual label function
				} // end drawlabels function
			} // end drawpie function
		} // end draw function
		
		// placed here because it needs to be accessed from multiple locations 
		function drawdonuthole(layer)
		{
			// draw donut hole
			if(options.series.pie.innerradius > 0)
			{
				// subtract the center
				layer.save();
				innerradius = options.series.pie.innerradius > 1 ? options.series.pie.innerradius : maxradius * options.series.pie.innerradius;
				layer.globalcompositeoperation = 'destination-out'; // this does not work with excanvas, but it will fall back to using the stroke color
				layer.beginpath();
				layer.fillstyle = options.series.pie.stroke.color;
				layer.arc(0,0,innerradius,0,math.pi*2,false);
				layer.fill();
				layer.closepath();
				layer.restore();
				
				// add inner stroke
				layer.save();
				layer.beginpath();
				layer.strokestyle = options.series.pie.stroke.color;
				layer.arc(0,0,innerradius,0,math.pi*2,false);
				layer.stroke();
				layer.closepath();
				layer.restore();
				// todo: add extra shadow inside hole (with a mask) if the pie is tilted.
			}
		}
		
		//-- additional interactive related functions --
		
		function ispointinpoly(poly, pt)
		{
			for(var c = false, i = -1, l = poly.length, j = l - 1; ++i < l; j = i)
				((poly[i][1] <= pt[1] && pt[1] < poly[j][1]) || (poly[j][1] <= pt[1] && pt[1]< poly[i][1]))
				&& (pt[0] < (poly[j][0] - poly[i][0]) * (pt[1] - poly[i][1]) / (poly[j][1] - poly[i][1]) + poly[i][0])
				&& (c = !c);
			return c;
		}
		
		function findnearbyslice(mousex, mousey)
		{
			var slices = plot.getdata(),
				options = plot.getoptions(),
				radius = options.series.pie.radius > 1 ? options.series.pie.radius : maxradius * options.series.pie.radius;
			
			for (var i = 0; i < slices.length; ++i) 
			{
				var s = slices[i];	
				
				if(s.pie.show)
				{
					ctx.save();
					ctx.beginpath();
					ctx.moveto(0,0); // center of the pie
					//ctx.scale(1, options.series.pie.tilt);	// this actually seems to break everything when here.
					ctx.arc(0,0,radius,s.startangle,s.startangle+s.angle,false);
					ctx.closepath();
					x = mousex-centerleft;
					y = mousey-centertop;
					if(ctx.ispointinpath)
					{
						if (ctx.ispointinpath(mousex-centerleft, mousey-centertop))
						{
							//alert('found slice!');
							ctx.restore();
							return {datapoint: [s.percent, s.data], dataindex: 0, series: s, seriesindex: i};
						}
					}
					else
					{
						// excanvas for ie doesn;t support ispointinpath, this is a workaround. 
						p1x = (radius * math.cos(s.startangle));
						p1y = (radius * math.sin(s.startangle));
						p2x = (radius * math.cos(s.startangle+(s.angle/4)));
						p2y = (radius * math.sin(s.startangle+(s.angle/4)));
						p3x = (radius * math.cos(s.startangle+(s.angle/2)));
						p3y = (radius * math.sin(s.startangle+(s.angle/2)));
						p4x = (radius * math.cos(s.startangle+(s.angle/1.5)));
						p4y = (radius * math.sin(s.startangle+(s.angle/1.5)));
						p5x = (radius * math.cos(s.startangle+s.angle));
						p5y = (radius * math.sin(s.startangle+s.angle));
						arrpoly = [[0,0],[p1x,p1y],[p2x,p2y],[p3x,p3y],[p4x,p4y],[p5x,p5y]];
						arrpoint = [x,y];
						// todo: perhaps do some mathmatical trickery here with the y-coordinate to compensate for pie tilt?
						if(ispointinpoly(arrpoly, arrpoint))
						{
							ctx.restore();
							return {datapoint: [s.percent, s.data], dataindex: 0, series: s, seriesindex: i};
						}			
					}
					ctx.restore();
				}
			}
			
			return null;
		}

		function onmousemove(e) 
		{
			triggerclickhoverevent('plothover', e);
		}
		
        function onclick(e) 
		{
			triggerclickhoverevent('plotclick', e);
        }

		// trigger click or hover event (they send the same parameters so we share their code)
		function triggerclickhoverevent(eventname, e) 
		{
			var offset = plot.offset(),
				canvasx = parseint(e.pagex - offset.left),
				canvasy =  parseint(e.pagey - offset.top),
				item = findnearbyslice(canvasx, canvasy);
			
			if (options.grid.autohighlight) 
			{
				// clear auto-highlights
				for (var i = 0; i < highlights.length; ++i) 
				{
					var h = highlights[i];
					if (h.auto == eventname && !(item && h.series == item.series))
						unhighlight(h.series);
				}
			}
			
			// highlight the slice
			if (item) 
			    highlight(item.series, eventname);
				
			// trigger any hover bind events
			var pos = { pagex: e.pagex, pagey: e.pagey };
			target.trigger(eventname, [ pos, item ]);	
		}

		function highlight(s, auto) 
		{
			if (typeof s == "number")
				s = series[s];

			var i = indexofhighlight(s);
			if (i == -1) 
			{
				highlights.push({ series: s, auto: auto });
				plot.triggerredrawoverlay();
			}
			else if (!auto)
				highlights[i].auto = false;
		}

		function unhighlight(s) 
		{
			if (s == null) 
			{
				highlights = [];
				plot.triggerredrawoverlay();
			}
			
			if (typeof s == "number")
				s = series[s];

			var i = indexofhighlight(s);
			if (i != -1) 
			{
				highlights.splice(i, 1);
				plot.triggerredrawoverlay();
			}
		}

		function indexofhighlight(s) 
		{
			for (var i = 0; i < highlights.length; ++i) 
			{
				var h = highlights[i];
				if (h.series == s)
					return i;
			}
			return -1;
		}

		function drawoverlay(plot, octx) 
		{
			//alert(options.series.pie.radius);
			var options = plot.getoptions();
			//alert(options.series.pie.radius);
			
			var radius = options.series.pie.radius > 1 ? options.series.pie.radius : maxradius * options.series.pie.radius;

			octx.save();
			octx.translate(centerleft, centertop);
			octx.scale(1, options.series.pie.tilt);
			
			for (i = 0; i < highlights.length; ++i) 
				drawhighlight(highlights[i].series);
			
			drawdonuthole(octx);

			octx.restore();

			function drawhighlight(series) 
			{
				if (series.angle < 0) return;
				
				//octx.fillstyle = parsecolor(options.series.pie.highlight.color).scale(null, null, null, options.series.pie.highlight.opacity).tostring();
				octx.fillstyle = "rgba(255, 255, 255, "+options.series.pie.highlight.opacity+")"; // this is temporary until we have access to parsecolor
				
				octx.beginpath();
				if (math.abs(series.angle - math.pi*2) > 0.000000001)
					octx.moveto(0,0); // center of the pie
				octx.arc(0,0,radius,series.startangle,series.startangle+series.angle,false);
				octx.closepath();
				octx.fill();
			}
			
		}	
		
	} // end init (plugin body)
	
	// define pie specific options and their default values
	var options = {
		series: {
			pie: {
				show: false,
				radius: 'auto',	// actual radius of the visible pie (based on full calculated radius if <=1, or hard pixel value)
				innerradius:0, /* for donut */
				startangle: 3/2,
				tilt: 1,
				offset: {
					top: 0,
					left: 'auto'
				},
				stroke: {
					color: '#fff',
					width: 1
				},
				label: {
					show: 'auto',
					formatter: function(label, slice){
						return '<div style="font-size:x-small;text-align:center;padding:2px;color:'+slice.color+';">'+label+'<br/>'+math.round(slice.percent)+'%</div>';
					},	// formatter function
					radius: 1,	// radius at which to place the labels (based on full calculated radius if <=1, or hard pixel value)
					background: {
						color: null,
						opacity: 0
					},
					threshold: 0	// percentage at which to hide the label (i.e. the slice is too narrow)
				},
				combine: {
					threshold: -1,	// percentage at which to combine little slices into one larger slice
					color: null,	// color to give the new slice (auto-generated if null)
					label: 'other'	// label to give the new slice
				},
				highlight: {
					//color: '#fff',		// will add this functionality once parsecolor is available
					opacity: 0.5
				}
			}
		}
	};
    
	$.plot.plugins.push({
		init: init,
		options: options,
		name: "pie",
		version: "1.0"
	});
})(jquery);
