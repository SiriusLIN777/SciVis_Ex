﻿// This source code is property of the Computer Graphics and Visualization chair of the
// TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved
//
// The main file of the plugin. It defines a class that demonstrates how to register with
// the scene graph, drawing primitives, creating a GUI, using a config file and various
// other parts of the framework.

// Framework core
#include <cgv/base/register.h>
#include <cgv/gui/provider.h>
#include <cgv/gui/trigger.h>
#include <cgv/render/drawable.h>
#include <cgv/render/shader_program.h>
#include <cgv/render/texture.h>
#include <cgv/render/frame_buffer.h>
#include <cgv/render/vertex_buffer.h>
#include <cgv/render/attribute_array_binding.h>
#include <cgv/math/ftransform.h>

// Framework standard plugins
#include <cgv_gl/gl/gl.h>

// Local includes
#include "cubes_fractal.h"

// Debug tools
#include "debug_macros.h"

// ************************************************************************************/
// Task 1.2a: Create a drawable that provides a (for now, empty) GUI and supports
//            reflection, so that its properties can be set via config file.
//
// Task 1.2b: Utilize the cubes_fractal class to render a fractal of hierarchically
//            transformed cubes. Expose its recursion depth and color properties to GUI
//            manipulation and reflection. Set reasonable values via the config
//            file.
//
// Task 1.2c: Implement an option (configurable via GUI and config file) to use a vertex
//            array object for rendering the cubes. The vertex array functionality 
//            should support (again, configurable via GUI and config file) both
//            interleaved (as in cgv_demo.cpp) and non-interleaved attributes.

// The CGV framework demonstration class
class cubes_drawable
	: public cgv::base::base,      // This class supports reflection
	public cgv::gui::provider,   // Instances of this class provde a GUI
	public cgv::render::drawable // Instances of this class can be rendered
{
protected:

	////
	// Stuff we expose via reflection

	std::string test;

	// Whether to use wireframe mode (helps visually debugging the custom tesselation task)
	bool wireframe;

	// Whether to draw the backside of the quad
	bool draw_backside;

	bool draw_my_quad;

	// Recursion depth
	int recursion_depth;
	int max_recursion_depth; // This can be set by both config and construct function

	// Primitive mode.
	enum PrimitiveMode {
		QUADS,
		TRIANGLES,
		TRIANGLE_STRIP} primitive_mode, temp;
	unsigned int GL_Primitive;

	////
	// Internal stuff we don't expose via reflection

	// Flag for checking whether we have to reinit due to change in desired offscreen
	// framebuffer resolution
	bool fb_invalid;

	cubes_fractal* fractal = new cubes_fractal();

	// Cube geometry buffers
	struct vertex_cube {
		cgv::vec3 pos;
		cgv::vec3 normal;
		cgv::vec4 color;
	};
	std::vector<vertex_cube> vertices_cube;
	cgv::render::vertex_buffer vb_cube;
	cgv::render::attribute_array_binding vertex_array_cube;

	float cube_color_r, cube_color_g, cube_color_b;
	cgv::rgba cube_color;

public:

	// Default constructor
	cubes_drawable()
		: fb_invalid(true),
		draw_backside(true), wireframe(false), draw_my_quad(true),
		primitive_mode(QUADS), GL_Primitive(GL_QUADS), cube_color(1.0f), recursion_depth(0), max_recursion_depth(8)
	{

	}

	// Should be overwritten to sensibly implement the cgv::base::named interface
	std::string get_type_name(void) const
	{
		return "dubes_drawable";
	}

	// Part of the cgv::base::base interface, can be implemented to make data members of
	// this class available as named properties, e.g. for use with config files
	bool self_reflect(cgv::reflect::reflection_handler& rh)
	{
		// Reflect the properties
		return
			rh.reflect_member("wireframe", wireframe) &&
			rh.reflect_member("draw_backside", draw_backside) &&
			rh.reflect_member("draw_my_quad", draw_my_quad) &&
			rh.reflect_member("primitive_mode", primitive_mode) &&
			rh.reflect_member("cube_color_r", cube_color_r) &&
			rh.reflect_member("cube_color_g", cube_color_g) &&
			rh.reflect_member("cube_color_b", cube_color_b) &&
			rh.reflect_member("recursion_depth", recursion_depth) &&
			rh.reflect_member("max_recursion_depth", max_recursion_depth);
	}

	// Part of the cgv::base::base interface, should be implemented to respond to write
	// access to reflected data members of this class, e.g. from config file processing
	// or gui interaction.
	void on_set(void* member_ptr)
	{
		// Reflect the change to cube_color_r/g/b in cube_color (can only happen via reflection)
		if (member_ptr == &cube_color_r || member_ptr == &cube_color_g ||
			member_ptr == &cube_color_b)
		{
			cube_color.R() = cube_color_r;
			cube_color.G() = cube_color_g;
			cube_color.B() = cube_color_b;
			update_member(&cube_color);
		}
		// ...and vice versa (can only happen via GUI interaction)
		if (member_ptr == &cube_color)
		{
			cube_color_r = cube_color.R();
			cube_color_g = cube_color.G();
			cube_color_b = cube_color.B();
		}
		//// Make sure the GUI reflects the new state, in case the write access did not
		//// originate from GUI interaction
		update_member(member_ptr);


		// Also trigger a redraw in case the drawable node is active
		if (this->is_visible())
			post_redraw();
	}

	// We use this for validating GUI input
	bool gui_check_value(cgv::gui::control<int>& ctrl)
	{
		// Check passed
		return true;
	}
	bool gui_check_value_primitive(cgv::gui::control<cubes_drawable::PrimitiveMode>& ctrl)
	{
		INFO("Now in gui_check_value_primitive");
		if (ctrl.controls(&primitive_mode))
		{
			DEBUG("Valid values: " << QUADS << ", " << TRIANGLES << ", " << TRIANGLE_STRIP);
			DEBUG("get_new_value: " << ctrl.get_new_value());
			if (ctrl.get_new_value() != QUADS
				&& ctrl.get_new_value() != TRIANGLES
				&& ctrl.get_new_value() != TRIANGLE_STRIP)
			{
				INFO("Primitive invalid: " << ctrl.get_new_value());
				return false;
			}
		}

		INFO("Primitive valid");
		temp = ctrl.get_new_value();
		// check passed
		return true;
	}
	bool gui_check_value_depth(cgv::gui::control<int>& ctrl)
	{
		INFO("Now in gui_check_value_depth");
		if (ctrl.controls(&recursion_depth))
		{
			DEBUG("get_new_value: " << ctrl.get_new_value());
			if (ctrl.get_new_value() < 0 || ctrl.get_new_value() > max_recursion_depth)
			{
				ERROR("Depth invalid");
				return false;
			}
		}

		INFO("Depth valid");
		// check passed
		return true;
	}

	// We use this for acting upon validated GUI input
	void gui_value_changed(cgv::gui::control<int>& ctrl)
	{
		//if (ctrl.controls(&fb_resolution.x()) || ctrl.controls(&fb_resolution.y()))
		//	// Indicate that the framebuffer must be reinitialized
		//	fb_invalid = true;

		// Redraw the scene
		post_redraw();
	}
	void gui_value_changed_primitive(cgv::gui::control<cubes_drawable::PrimitiveMode>& ctrl)
	{
		INFO("Now in gui_value_changed_primitive ...");
		DEBUG("ctrl.controls(&primitive_mode): " << ctrl.get_new_value());
		if (ctrl.controls(&primitive_mode))
		{
			INFO("Now,changing the GL_Primitive...");
			DEBUG("gui_value_changed: ctrl.get_new_value(): " << ctrl.get_new_value());
			switch (ctrl.controls(&primitive_mode)) {
			case QUADS:
			{
				INFO("Mode -> GL_QUADS");
				GL_Primitive = GL_QUADS;
				break;
			}
			case TRIANGLES:
			{
				INFO("Mode -> GL_TRIANGLES");
				GL_Primitive = GL_TRIANGLES;
				break;
			}
			case TRIANGLE_STRIP:
			{
				INFO("Mode -> GL_TRIANGLE_STRIP");
				GL_Primitive = GL_TRIANGLE_STRIP;
				break;
			}
			default:
			{
				INFO("Mode default -> GL_QUADS");
				GL_Primitive = GL_QUADS;
				INFO("Unsupported Primitive TYPE: " << ctrl.get_new_value());
			}
			DEBUG(" after switch case, GL_Primitve: " << GL_Primitive);
			}

		}
		INFO("GL_Primitive: " << GL_Primitive);

		// Redraw the scene
		post_redraw();
	}
	void gui_value_changed_depth(cgv::gui::control<int>& ctrl)
	{
		if (ctrl.controls(&recursion_depth))
			DEBUG("recursion depth changed: "<< ctrl.controls(&recursion_depth));

		// Redraw the scene
		post_redraw();
	}

	// Required interface for cgv::gui::provider
	void create_gui(void)
	{
		// There are currently no vector controls, so add each component individually as
		// a slider. Also, we obtain the actual control object to do more advanced
		// callback logic than the implicit on_set() above.
		// - section header
		add_decorator("Offscreen framebuffer", "heading", "level=1");

		//  Finally, simple controls for the switch to draw wireframes
		add_member_control(this, "wireframe rendering", wireframe);

		// ... and for the switch to disable drawing the backside of the quad.
		add_member_control(this, "draw backside", draw_backside);

		// Settings for cube
		add_decorator("Cube settings", "heading", "level=1");
		// Control for setting recursion
		cgv::gui::control<int>* ctrl_recursion = add_control(
			"recursion depth", recursion_depth, "value_slider",
			"min=0;max=" + std::to_string(max_recursion_depth - 1) + ";ticks=false"
		).operator->();
		cgv::signal::connect(ctrl_recursion->check_value, this, &cubes_drawable::gui_check_value_depth);
		cgv::signal::connect(ctrl_recursion->value_change, this, &cubes_drawable::gui_value_changed_depth);

		// Control for GL primitive mode selection
		cgv::gui::control<PrimitiveMode>* ctrl_primitive = add_control(
			"primitive mode", primitive_mode, "dropdown",
			"enums='QUADS,TRIANGLES,TRIANGLE_STRIP'"
		).operator->();
		cgv::signal::connect(ctrl_primitive->check_value, this, &cubes_drawable::gui_check_value_primitive);
		cgv::signal::connect(ctrl_primitive->value_change, this, &cubes_drawable::gui_value_changed_primitive);

		add_member_control(this, "cube color", cube_color);
	}



	// Part of the cgv::render::drawable interface, can be overwritten if there is some
	// intialization work to be done that needs a set-up and ready graphics context,
	// which usually you don't have at object construction time. Should return true if
	// the initialization was successful, false otherwise.
	bool init(cgv::render::context& ctx)
	{
		// Keep track of success - do it this way (instead of e.g. returning false
		// immediatly) to perform every init step even if some go wrong.
		bool success = true;

		// Init geometry buffers
		// - get a reference to the default shader, from which we're going to query named
		//   locations of the vertex layout
		cgv::render::shader_program& surface_shader
			= ctx.ref_surface_shader_program(true /* true for texture support */);
		// - generate actual geometry
		init_unit_cube_geometry();
		// - obtain type descriptors for the automatic array binding facilities of the
		//   framework
		cgv::render::type_descriptor
			vec3type_normal =
			cgv::render::element_descriptor_traits<cgv::vec3>
			::get_type_descriptor(vertices_cube[0].normal),
			vec3type_pos =
			cgv::render::element_descriptor_traits<cgv::vec3>
			::get_type_descriptor(vertices_cube[0].pos),
			vec4type_color =
			cgv::render::element_descriptor_traits<cgv::vec4>
			::get_type_descriptor(vertices_cube[0].color);
		// - create buffer objects
		success = vb_cube.create(ctx, &(vertices_cube[0]), vertices_cube.size()) && success;
		success = vertex_array_cube.create(ctx) && success;

		// Set position.
		success = vertex_array_cube.set_attribute_array(
			ctx, surface_shader.get_position_index(), vec3type_pos, vb_cube,
			0, // position is at start of the struct <-> offset = 0
			vertices_cube.size(), // number of position elements in the array
			sizeof(vertex_cube) // stride from one element to next
		) && success;
		// Set normal.
		success = vertex_array_cube.set_attribute_array(
			ctx, surface_shader.get_normal_index(), vec3type_normal, vb_cube,
			sizeof(cgv::vec3), // normal coords follow after position
			vertices_cube.size(), // number of normal elements in the array
			sizeof(vertex_cube) // stride from one element to next
		) && success;
		// Set color.
		success = vertex_array_cube.set_attribute_array(
			ctx, surface_shader.get_color_index(), vec4type_color, vb_cube,
			sizeof(cgv::vec3)*2, // color vec follow after pos and mormal
			vertices_cube.size(), // number of color elements in the array
			sizeof(vertex_cube) // stride from one element to next
		) && success;

		// Flag offscreen framebuffer as taken care of
		fb_invalid = false;

		// All initialization has been attempted
		return success;
	}

	// Part of the cgv::render::drawable interface, can be overwritten if there is some
	// work to be done before actually rendering a frame.
	void init_frame(cgv::render::context& ctx)
	{
		// Check if we need to recreate anything
		if (fb_invalid)
		{
			init(ctx);
		}
	}

	// Should be overwritten to sensibly implement the cgv::render::drawable interface
	void draw(cgv::render::context& ctx)
	{
		////
		// Draw the contents of this node.

		// Observe wireframe mode
		glPushAttrib(GL_POLYGON_BIT);
		if (wireframe)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		// Shortcut to the built-in default shader with lighting and texture support
		cgv::render::shader_program& default_shader =
			ctx.ref_surface_shader_program(true /* true for texture support */);

		// Enable shader program we want to use for drawing
		default_shader.enable(ctx);

		draw_my_unit_square(ctx);

		// Restore the attributes.
		glPopAttrib();

		// Disable shader program
		default_shader.disable(ctx);
	}

	// Creates geometry for the cube
	void init_unit_cube_geometry(void)
	{
		// Prepare array
		vertices_cube.resize(36);
		// Set the Points.

/*
		// GL_TRIANGLE_STRIP
		vertices_cube[0].pos = cgv::vec3(-1, 1, -1);
		vertices_cube[1].pos = cgv::vec3(-1, -1, -1);
		vertices_cube[2].pos = cgv::vec3(-1, 1, 1);
		vertices_cube[3].pos = cgv::vec3(-1, -1, 1);
		vertices_cube[4].pos = cgv::vec3(1, 1, 1);
		vertices_cube[5].pos = cgv::vec3(1, -1, 1); 

		vertices_cube[0].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[1].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[2].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[3].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[4].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[5].normal = cgv::vec3(-1, 0, 0);

		vertices_cube[6].pos = cgv::vec3(-1, -1, 1);
		vertices_cube[7].pos = cgv::vec3(1, -1, -1);
		vertices_cube[8].pos = cgv::vec3(-1, -1, -1);
		vertices_cube[9].pos = cgv::vec3(-1, -1, -1);
		vertices_cube[10].pos = cgv::vec3(1, -1, -1);
		vertices_cube[11].pos = cgv::vec3(1, 1, -1);

		vertices_cube[6].normal = cgv::vec3(0, 0, 1);
		vertices_cube[7].normal = cgv::vec3(0, 0, 1);
		vertices_cube[8].normal = cgv::vec3(0, 0, 1);
		vertices_cube[9].normal = cgv::vec3(0, 0, 1);
		vertices_cube[10].normal = cgv::vec3(0, 0, 1);
		vertices_cube[11].normal = cgv::vec3(0, 0, 1);

		vertices_cube[12].pos = cgv::vec3(-1, 1, -1);
		vertices_cube[13].pos = cgv::vec3(-1, 1, -1);
		vertices_cube[14].pos = cgv::vec3(1, 1, -1);
		vertices_cube[15].pos = cgv::vec3(1, 1, 1);
		vertices_cube[16].pos = cgv::vec3(1, -1, -1);
		vertices_cube[17].pos = cgv::vec3(1, -1, 1);

		vertices_cube[12].normal = cgv::vec3(0, -1, 0);
		vertices_cube[13].normal = cgv::vec3(0, -1, 0);
		vertices_cube[14].normal = cgv::vec3(0, -1, 0);
		vertices_cube[15].normal = cgv::vec3(0, -1, 0);
		vertices_cube[16].normal = cgv::vec3(0, -1, 0);
		vertices_cube[17].normal = cgv::vec3(0, -1, 0);
*/


		/// GL_TRIANGLES
		//Left face
		vertices_cube[0].pos = cgv::vec3(-1, -1, -1);
		vertices_cube[1].pos = cgv::vec3(-1, -1, 1);
		vertices_cube[2].pos = cgv::vec3(-1, 1, 1);
		vertices_cube[3].pos = cgv::vec3(-1, 1, 1);
		vertices_cube[4].pos = cgv::vec3(-1, 1, -1);
		vertices_cube[5].pos = cgv::vec3(-1, -1, -1);

		vertices_cube[0].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[1].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[2].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[3].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[4].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[5].normal = cgv::vec3(-1, 0, 0);

		// Top face
		vertices_cube[6].pos = cgv::vec3(-1, -1, 1);
		vertices_cube[7].pos = cgv::vec3(1, -1, 1);
		vertices_cube[8].pos = cgv::vec3(1, 1, 1);
		vertices_cube[9].pos = cgv::vec3(1, 1, 1);
		vertices_cube[10].pos = cgv::vec3(-1, 1, 1);
		vertices_cube[11].pos = cgv::vec3(-1, -1, 1);

		vertices_cube[6].normal = cgv::vec3(0, 0, 1);
		vertices_cube[7].normal = cgv::vec3(0, 0, 1);
		vertices_cube[8].normal = cgv::vec3(0, 0, 1);
		vertices_cube[9].normal = cgv::vec3(0, 0, 1);
		vertices_cube[10].normal = cgv::vec3(0, 0, 1);
		vertices_cube[11].normal = cgv::vec3(0, 0, 1);

		// Front face
		vertices_cube[12].pos = cgv::vec3(-1, -1, -1);
		vertices_cube[13].pos = cgv::vec3(1, -1, -1);
		vertices_cube[14].pos = cgv::vec3(1, -1, 1);
		vertices_cube[15].pos = cgv::vec3(1, -1, 1);
		vertices_cube[16].pos = cgv::vec3(-1, -1, 1);
		vertices_cube[17].pos = cgv::vec3(-1, -1, -1);

		vertices_cube[12].normal = cgv::vec3(0, -1, 0);
		vertices_cube[13].normal = cgv::vec3(0, -1, 0);
		vertices_cube[14].normal = cgv::vec3(0, -1, 0);
		vertices_cube[15].normal = cgv::vec3(0, -1, 0);
		vertices_cube[16].normal = cgv::vec3(0, -1, 0);
		vertices_cube[17].normal = cgv::vec3(0, -1, 0);

		// Bottom face
		vertices_cube[18].pos = cgv::vec3(-1, -1, -1);
		vertices_cube[19].pos = cgv::vec3(-1, 1, -1);
		vertices_cube[20].pos = cgv::vec3(1, 1, -1);
		vertices_cube[21].pos = cgv::vec3(1, 1, -1);
		vertices_cube[22].pos = cgv::vec3(1, -1, -1);
		vertices_cube[23].pos = cgv::vec3(-1, -1, -1);

		vertices_cube[18].normal = cgv::vec3(0, 0, -1);
		vertices_cube[19].normal = cgv::vec3(0, 0, -1);
		vertices_cube[20].normal = cgv::vec3(0, 0, -1);
		vertices_cube[21].normal = cgv::vec3(0, 0, -1);
		vertices_cube[22].normal = cgv::vec3(0, 0, -1);
		vertices_cube[23].normal = cgv::vec3(0, 0, -1);

		
		// Back face
		vertices_cube[24].pos = cgv::vec3(-1, 1, -1);
		vertices_cube[25].pos = cgv::vec3(-1, 1, 1);
		vertices_cube[26].pos = cgv::vec3(1, 1, 1);
		vertices_cube[27].pos = cgv::vec3(1, 1, 1);
		vertices_cube[28].pos = cgv::vec3(1, 1, -1);
		vertices_cube[29].pos = cgv::vec3(-1, 1, -1);

		vertices_cube[24].normal = cgv::vec3(0, 1, 0);
		vertices_cube[25].normal = cgv::vec3(0, 1, 0);
		vertices_cube[26].normal = cgv::vec3(0, 1, 0);
		vertices_cube[27].normal = cgv::vec3(0, 1, 0);
		vertices_cube[28].normal = cgv::vec3(0, 1, 0);
		vertices_cube[29].normal = cgv::vec3(0, 1, 0);

		//Right face
		vertices_cube[30].pos = cgv::vec3(1, -1, -1);
		vertices_cube[31].pos = cgv::vec3(1, 1, -1);
		vertices_cube[32].pos = cgv::vec3(1, 1, 1);
		vertices_cube[33].pos = cgv::vec3(1, 1, 1);
		vertices_cube[34].pos = cgv::vec3(1, -1, 1);
		vertices_cube[35].pos = cgv::vec3(1, -1, -1);

		vertices_cube[30].normal = cgv::vec3(1, 0, 0);
		vertices_cube[31].normal = cgv::vec3(1, 0, 0);
		vertices_cube[32].normal = cgv::vec3(1, 0, 0);
		vertices_cube[33].normal = cgv::vec3(1, 0, 0);
		vertices_cube[34].normal = cgv::vec3(1, 0, 0);
		vertices_cube[35].normal = cgv::vec3(1, 0, 0);



/*
		/// GL_QUADS		
		//Front face
		vertices_cube[0].pos = cgv::vec3(-1, -1, 1);
		vertices_cube[1].pos = cgv::vec3(1, -1, 1);
		vertices_cube[2].pos = cgv::vec3(1, 1, 1);
		vertices_cube[3].pos = cgv::vec3(-1, 1, 1);

		vertices_cube[0].normal = cgv::vec3(0, 0, 1);
		vertices_cube[1].normal = cgv::vec3(0, 0, 1);
		vertices_cube[2].normal = cgv::vec3(0, 0, 1);
		vertices_cube[3].normal = cgv::vec3(0, 0, 1);
		
		//Back face
		vertices_cube[4].pos = cgv::vec3(-1, -1, -1);
		vertices_cube[5].pos = cgv::vec3(-1, 1, -1);
		vertices_cube[6].pos = cgv::vec3(1, 1, -1);
		vertices_cube[7].pos = cgv::vec3(1, -1, -1);
		
		vertices_cube[4].normal = cgv::vec3(0, 0, -1);
		vertices_cube[5].normal = cgv::vec3(0, 0, -1);
		vertices_cube[6].normal = cgv::vec3(0, 0, -1);
		vertices_cube[7].normal = cgv::vec3(0, 0, -1);
		
		//Left face
		vertices_cube[8].pos = cgv::vec3(-1, -1, -1);
		vertices_cube[9].pos = cgv::vec3(-1, -1, 1);
		vertices_cube[10].pos = cgv::vec3(-1, 1, 1);
		vertices_cube[11].pos = cgv::vec3(-1, 1, -1);
		
		vertices_cube[8].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[9].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[10].normal = cgv::vec3(-1, 0, 0);
		vertices_cube[11].normal = cgv::vec3(-1, 0, 0);
		
		//Right face
		vertices_cube[12].pos = cgv::vec3(1, -1, -1);
		vertices_cube[13].pos = cgv::vec3(1, 1, -1);
		vertices_cube[14].pos = cgv::vec3(1, 1, 1);
		vertices_cube[15].pos = cgv::vec3(1, -1, 1);
		
		vertices_cube[12].normal = cgv::vec3(1, 0, 0);
		vertices_cube[13].normal = cgv::vec3(1, 0, 0);
		vertices_cube[14].normal = cgv::vec3(1, 0, 0);
		vertices_cube[15].normal = cgv::vec3(1, 0, 0);
		
		//Top face
		vertices_cube[16].pos = cgv::vec3(-1, 1, -1);
		vertices_cube[17].pos = cgv::vec3(-1, 1, 1);
		vertices_cube[18].pos = cgv::vec3(1, 1, 1);
		vertices_cube[19].pos = cgv::vec3(1, 1, -1);
		
		vertices_cube[16].normal = cgv::vec3(0, 1, 0);
		vertices_cube[17].normal = cgv::vec3(0, 1, 0);
		vertices_cube[18].normal = cgv::vec3(0, 1, 0);
		vertices_cube[19].normal = cgv::vec3(0, 1, 0);
		
		//Bottom face
		vertices_cube[20].pos = cgv::vec3(-1, -1, -1);
		vertices_cube[21].pos = cgv::vec3(1, -1, -1);
		vertices_cube[22].pos = cgv::vec3(1, -1, 1);
		vertices_cube[23].pos = cgv::vec3(-1, -1, 1);
	
		vertices_cube[20].normal = cgv::vec3(0, -1, 0);
		vertices_cube[21].normal = cgv::vec3(0, -1, 0);
		vertices_cube[22].normal = cgv::vec3(0, -1, 0);
		vertices_cube[23].normal = cgv::vec3(0, -1, 0);
*/
	}

	// Draw method for a custom quad
	void draw_my_unit_square(cgv::render::context& ctx)
	{
		INFO("Now in draw_my_unit_sqaure...");
		DEBUG(" Drawing, GL_Primitive: " << GL_Primitive);
		DEBUG("Drawing, recursion_depth: " << recursion_depth << ", max_recursion_depth: " << max_recursion_depth);
		fractal->use_vertex_array(&vertex_array_cube, vertices_cube.size(), GL_Primitive);
		fractal->draw_recursive(ctx, cube_color, recursion_depth);
	}

};

// [END] Tasks 1.2a, 1.2b and 1.2c
// ************************************************************************************/


// ************************************************************************************/
// Task 1.2a: register an instance of your drawable.

// Create an instance of the demo class at plugin load and register it with the framework
cgv::base::object_registration<cubes_drawable> cubes_drawable_registration("cubes_drawable");
