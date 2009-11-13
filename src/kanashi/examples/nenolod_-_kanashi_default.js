/*
 * kanashi_test.js
 *
 * Test visualization scene for Kanashi engine.
 */

set_colormap_gradient(0, 255, 0, 192, 255);

render_scene = function() {
	// render left and right channels in the middle as two waves.
	render_horizontal_waveform(-1, 255);
	render_horizontal_waveform(1, 255);

	if (is_beat()) {
		blur(Math.floor(Math.random() * 4));
		value_invert();
		value_reduce(1);
		set_colormap_gradient(0, 255,
					Math.floor(Math.random() * 255),
					Math.floor(Math.random() * 255),
					Math.floor(Math.random() * 255));
	}

	blur(1);

	// reduce value by 2
	value_reduce(2);
}
