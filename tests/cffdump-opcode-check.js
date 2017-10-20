var path = require("path");
var fs = require("fs");
var util = require("util");

function check (demand,fn, desc) {
	if (fn) {
		process.stderr.write("\x1b[32;1m[PASS]\x1b[39;49m " + desc + "\n");
	} else {
		process.stderr.write("\x1b[31;1m[FAIL]\x1b[39;49m " + desc + "\n");
		process.stderr.write(util.inspect(demand, {depth: null}));
		process.exit(1);
	}
}
function near (x, y) {
	return Math.abs(x - y) < 0.01;
}

var injson = "";
process.stdin.resume();
process.stdin.on("data", function (buf) { injson += buf.toString(); });
process.stdin.on("end", function () {
	var infont = JSON.parse(injson.trim());
	var rect = infont.glyf.rectangle;
	check(rect, rect, "The rectangle glyph exists.");
	check(rect, rect.advanceWidth === 700, "The rectangle glyph's width is 700.");
	check(rect, rect.contours.length === 1, "The rectangle has one contour.");
	check(rect, rect.contours[0].length === 4, "The rectangle has one contour with four points.");
	check(rect, near(0, rect.contours[0][0].x) && near(0, rect.contours[0][0].y), "point 1 is (0, 0).");
	check(rect, near(0, rect.contours[0][1].x) && near(700, rect.contours[0][1].y), "point 2 is (0, 700).");
	check(rect, near(700, rect.contours[0][2].x) && near(700, rect.contours[0][2].y), "point 3 is (700, 700).");
	check(rect, near(700, rect.contours[0][3].x) && near(0, rect.contours[0][3].y), "point 4 is (700, 0).");
});
