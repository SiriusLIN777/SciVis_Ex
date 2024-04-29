// This source code is property of the Computer Graphics and Visualization chair of the
// TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved
//
// The main file of the plugin. It defines a class that demonstrates how to register with
// the scene graph, drawing primitives, creating a GUI, using a config file and various
// other parts of the framework.

#define DEBUG_MODE
#define INFO_MODE
#define ERROR_MODE

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
#include <chrono>

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

// ************************************************************************************/
// 
// &&& NOTE: The registration of cgv.demo is commented out.
// 
// Task 1.2 a (√)
// Task 1.2 b (√)
// Task 1.2 c (√)
// Task 1.3.1 a (√): Exp ev. : recursion level-1， repeat render 10 times.
//					  | round\time | All_in_One | Unit Cube | 
//                    ---------------------------------------
//                    |   1    \      1722 ms   |  1928 ms  |
//                    |   2     \     1649 ms   |  1485 ms  |
//                    |   3      \    1861 ms   |  1640 ms  |
//                    |   4       \   2276 ms   |  1598 ms  |
//                    |   5        |  1781 ms   |  1629 ms  |
//                    |   6        |  1519 ms   |  1892 ms  |
//                    |   7        |  1544 ms   |  1910 ms  |
//                    |   8        |  1491 ms   |  1930 ms  |
//                    |   9        |  1419 ms   |  1766 ms  |
//                    |  10        |  1514 ms   |  1525 ms  |
//                    |  AVG       |  1678 ms   |  1714 ms  |
//        Conclusion: The rendering time of all_in_one geometry is less than unit geometry, 
//                    the possible reason can be less draw-call used for all_in_one 
//                    geometry, and for unit geometry each cube needs one draw-call.


// The CGV framework demonstration class
class cubes_drawable
	: public cgv::base::base,      // This class supports reflection
	public cgv::gui::provider,   // Instances of this class provde a GUI
	public cgv::render::drawable // Instances of this class can be rendered
{
protected:

	////
	// Stuff we expose via reflection

	// Cube vertices size
	int vertices_cube_size;

	// Whether to use wireframe mode (helps visually debugging the custom tesselation task)
	bool wireframe;

	// Whether to draw the backside of the quad
	bool draw_backside;

	// Recursion depth
	int recursion_depth;
	int max_recursion_depth; // This can be set by both config and construct function

	// Primitive mode.
	enum PrimitiveMode {
		QUADS,
		TRIANGLES,
		TRIANGLE_STRIP
	} primitive_mode, primitive;
	unsigned int GL_Primitive;

	// (non-)interleaved mode selection enum
	// INTERLEAVED = 0, NON_INTERLEAVED_SINGLE_VBO = 1, BUILTIN = 2, ALL_in_ONE = 3
	enum InterleavedMode {
		INTERLEAVED,
		NON_INTERLEAVED_SINGLE_VBO,
		BUILTIN,
		ALL_in_ONE
	} interleaved_mode, interleaved_tmp;
	int interleaved;
	
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

	// Vertices for interleaved mode.
	std::vector<vertex_cube> vertices_cube;
	// Vertices for non-interleaved mode.
	std::vector<cgv::vec3> vertices_cube_pos;
	std::vector<cgv::vec3> vertices_cube_normal;
	std::vector<cgv::vec4> vertices_cube_color;
	std::vector<cgv::vec3> vertices_sVBO;
	// Vertices for all in one geometry.
	std::vector<vertex_cube> vertices_AiO;

	// Vertex buffer.
	// ... for interleaved mode
	cgv::render::vertex_buffer vb_cube; 
	cgv::render::attribute_array_binding vertex_array_cube;
	// ... for non-interleaved mode
	cgv::render::vertex_buffer vb_cube_sVBO;
	cgv::render::attribute_array_binding vertex_array_cube_sVBO;
	// ... for all in one 
	cgv::render::vertex_buffer vb_AiO;
	cgv::render::attribute_array_binding va_AiO;

	//// Multi VBO
	//cgv::render::vertex_buffer vb_cube_mVBO_pos;
	//cgv::render::vertex_buffer vb_cube_mVBO_normal;
	//cgv::render::vertex_buffer vb_cube_mVBO_color;
	//cgv::render::attribute_array_binding vertex_array_cube_mVBO_pos;
	//cgv::render::attribute_array_binding vertex_array_cube_mVBO_normal;
	//cgv::render::attribute_array_binding vertex_array_cube_mVBO_color;

	// Used to pass reflectance information to shaders for coloring the cubes
	cgv::media::illum::surface_material material;

	float cube_color_r, cube_color_g, cube_color_b;
	cgv::rgba cube_color;

	// Array to store num of cubes in each level, and init to be 0.
	std::vector<int> num_level;
	

public:

	// Default constructor
	cubes_drawable()
		: vertices_cube_size(36),
		fb_invalid(true),
		draw_backside(true), wireframe(false),
		primitive_mode(TRIANGLES),primitive(TRIANGLES), GL_Primitive(GL_QUADS),
		cube_color_r(0.0f), cube_color_g(0.2f), cube_color_b(0.9f), cube_color(cube_color_r, cube_color_g, cube_color_b),
		recursion_depth(1), max_recursion_depth(8),
		interleaved_mode(NON_INTERLEAVED_SINGLE_VBO), interleaved_tmp(NON_INTERLEAVED_SINGLE_VBO), interleaved(NON_INTERLEAVED_SINGLE_VBO)
	{
		material.set_brdf_type(
			(cgv::media::illum::BrdfType)(cgv::media::illum::BT_LAMBERTIAN | cgv::media::illum::BT_PHONG)
		);
		material.ref_specular_reflectance() = { .0625f, .0625f, .0625f };
		material.ref_roughness() = .03125f;
	}

	// Should be overwritten to sensibly implement the cgv::base::named interface
	std::string get_type_name(void) const
	{
		return "cubes_drawable";
	}

	// Part of the cgv::base::base interface, can be implemented to make data members of
	// this class available as named properties, e.g. for use with config files
	bool self_reflect(cgv::reflect::reflection_handler& rh)
	{
		// Reflect the properties
		return
			rh.reflect_member("wireframe", wireframe) &&
			rh.reflect_member("draw_backside", draw_backside) &&
			rh.reflect_member("primitive_mode", primitive_mode) &&
			rh.reflect_member("cube_color_r", cube_color_r) &&
			rh.reflect_member("cube_color_g", cube_color_g) &&
			rh.reflect_member("cube_color_b", cube_color_b) &&
			rh.reflect_member("recursion_depth", recursion_depth) &&
			rh.reflect_member("max_recursion_depth", max_recursion_depth) &&
			rh.reflect_member("interleaved_mode", interleaved_mode);
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

		// Primitive control.
		if (member_ptr == &primitive_mode)
		{
			primitive = primitive_mode;
			update_member(&primitive);
		}
		if (member_ptr == &primitive)
		{
			primitive_mode = primitive;
		}
		update_member(member_ptr);

		// Interleaved control.
		if (member_ptr == &interleaved_mode)
		{
			interleaved_tmp = interleaved_mode;
			update_member(&interleaved_tmp);
		}
		if (member_ptr == &interleaved_tmp)
		{
			interleaved_mode = interleaved_tmp;
		}
		update_member(member_ptr);

		// Also trigger a redraw in case the drawable node is active
		if (this->is_visible())
			post_redraw();
	}

	// We use this for validating GUI input
	bool gui_check_value_primitive(cgv::gui::control<cubes_drawable::PrimitiveMode>& ctrl)
	{
		INFO("Now in gui_check_value_primitive");
		if (ctrl.controls(&primitive_mode))
		{
			DEBUG("Valid primitive values: " << QUADS << ", " << TRIANGLES << ", " << TRIANGLE_STRIP);
			DEBUG("primitive get_new_value: " << ctrl.get_new_value());
			
			if (ctrl.get_new_value() != QUADS
				&& ctrl.get_new_value() != TRIANGLES
				&& ctrl.get_new_value() != TRIANGLE_STRIP)
			{
				INFO("Primitive invalid: " << ctrl.get_new_value());
				return false;
			}
		}

		INFO("Primitive valid");
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
	/*bool gui_check_value_interleaved(cgv::gui::control<cubes_drawable::InterleavedMode>& ctrl)
	{
		INFO("Now in gui_check_value_interleaved.");
		if (ctrl.controls(&interleaved_mode))
		{
			DEBUG("Valid interleaved values: " << INTERLEAVED << ", " << SINGLE_VBO << ", " << MULTI_VBO);
			DEBUG("interleaved get_new_value: " << ctrl.get_new_value());
			if (ctrl.get_new_value() != INTERLEAVED
				&& ctrl.get_new_value() != SINGLE_VBO
				&& ctrl.get_new_value() != MULTI_VBO)
			{
				INFO("Interleaved mode invalid: " << ctrl.get_new_value());
				return false;
			}
		}

		INFO("Interleaved mode valid");
		// check passed
		return true;
	}*/

	// We use this for acting upon validated GUI input
	void gui_value_changed_primitive(cgv::gui::control<cubes_drawable::PrimitiveMode>& ctrl)
	{
		INFO("Now in gui_value_changed_primitive ...");
		DEBUG("ctrl.controls(&primitive_mode): " << ctrl.controls(&primitive_mode));
		DEBUG("primitive ctrl.get_new_value(): " << ctrl.get_new_value());
		if (ctrl.controls(&primitive_mode))
		{
			INFO("Now,changing the GL_Primitive...");
			auto tmp = ctrl.controls(&primitive_mode);
			DEBUG("primitive tmp: " << tmp);
			switch (ctrl.get_new_value()) {
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
		{
			DEBUG("recursion depth changed: " << ctrl.controls(&recursion_depth));
			DEBUG("old depth:" << ctrl.get_old_value() << ", new depth:" << ctrl.get_new_value() << ", value:" << ctrl.get_value());
			if (ctrl.get_old_value() != ctrl.get_value())
			{
				fb_invalid = true;
			}
		}

		// Redraw the scene
		post_redraw();
	}
	/*void gui_value_changed_interleaved(cgv::gui::control<cubes_drawable::InterleavedMode>& ctrl)
	{
		INFO("Now in gui_value_changed_interleaved.");
		DEBUG("ctrl.controls(&interleaved_mode): " << ctrl.controls(&interleaved_mode));
		DEBUG("interleaved ctrl.get_new_value(): " << ctrl.get_new_value());
		if (ctrl.controls(&interleaved_mode))
		{
			INFO("Now,changing the GL_Primitive...");
			switch (ctrl.get_new_value()) {
			case INTERLEAVED:
			{
				INFO("Mode -> INTERLEAVED");
				interleaved = 0;
				break;
			}
			case SINGLE_VBO:
			{
				INFO("Mode -> SINGLE_VBO");
				interleaved = 1;
				break;
			}
			case MULTI_VBO:
			{
				INFO("Mode -> MULTI_VBO");
				interleaved = 2;
				break;
			}
			default:
			{
				INFO("Mode default -> INTERLEAVED");
				interleaved = 0;
				ERROR("Unsupported interleaved TYPE: " << ctrl.get_new_value());
			}
			DEBUG("[ 0 = INTERLEAVED; 1 = SINGLE_VBO; 2 = MULTI_VBO ] after switch case, interleaved: " << interleaved);
			}

		}
		INFO("interleaved: " << interleaved);

		// Redraw the scene
		post_redraw();
	}*/


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

		// ... for GL primitive mode selection
		/*
		cgv::gui::control<PrimitiveMode>* ctrl_primitive = add_control(
			"primitive mode", primitive_mode, "dropdown",
			"enums='QUADS,TRIANGLES,TRIANGLE_STRIP'"
		).operator->();
		cgv::signal::connect(ctrl_primitive->check_value, this, &cubes_drawable::gui_check_value_primitive);
		cgv::signal::connect(ctrl_primitive->value_change, this, &cubes_drawable::gui_value_changed_primitive);
		*/
		add_member_control(
			this, "primitive mode", primitive_mode, "dropdown",
			"enums='QUADS,TRIANGLES,TRIANGLE_STRIP'"
		);

		// ... for switch interleaved/non-interleaved(Single/Multi VBO) mode.
		// INTERLEAVED = 0, SINGLE_VBO = 1, MULTI_VBO = 2
		/*
		cgv::gui::control<InterleavedMode>* ctrl_interleaved = add_control(
			"interleaved mode", interleaved_mode, "dropdown",
			"enums='INTERLEAVED,SINGLE_VBO,MULTI_VBO'"
		).operator->();
		cgv::signal::connect(ctrl_interleaved->check_value, this, &cubes_drawable::gui_check_value_interleaved);
		cgv::signal::connect(ctrl_interleaved->value_change, this, &cubes_drawable::gui_value_changed_interleaved);
		*/
		add_member_control(
			this, "interleaved mode", interleaved_mode, "dropdown",
			"enums='INTERLEAVED,NON_INTERLEAVED_SINGLE_VBO,BUILTIN,ALL_in_ONE'"
		);

		// ... for change cube color
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
		
		/* if (interleaved_mode == INTERLEAVED)
		{
					
		}
		else if (interleaved_mode == NON_INTERLEAVED_SINGLE_VBO)
		{
			
		}
		else if (interleaved_mode == MULTI_VBO)
		{
			// - non-interleaved multi vbo.
			cgv::render::type_descriptor
				vec3type_mVBO_pos =
				cgv::render::element_descriptor_traits<cgv::vec3>
				::get_type_descriptor(vertices_cube_pos[0]),
				vec3type_mVBO_normal =
				cgv::render::element_descriptor_traits<cgv::vec3>
				::get_type_descriptor(vertices_cube_normal[0]),
				vec4type_mVBO_color =
				cgv::render::element_descriptor_traits<cgv::vec4>
				::get_type_descriptor(vertices_cube_color[0]);

			success = vb_cube_mVBO_pos.create(ctx, &(vertices_cube_pos[0]), vertices_cube_pos.size()) && success;
			success = vertex_array_cube_mVBO_pos.create(ctx) && success;

			success = vb_cube_mVBO_normal.create(ctx, &(vertices_cube_normal[0]), vertices_cube_normal.size()) && success;
			success = vertex_array_cube_mVBO_normal.create(ctx) && success;

			success = vb_cube_mVBO_color.create(ctx, &(vertices_cube_color[0]), vertices_cube_color.size()) && success;
			success = vertex_array_cube_mVBO_color.create(ctx) && success;

			// Set attribute array - Interleaved - Multi VBO
			// Set position.
			success = vertex_array_cube_mVBO_pos.set_attribute_array(
				ctx, surface_shader.get_position_index(), vec3type_mVBO_pos, vb_cube_mVBO_pos,
				0, // no offset
				vertices_cube_pos.size(), // number of position elements in the array
				sizeof(cgv::vec3) // no stride for multi vbo
			) && success;
			// Set normal.
			success = vertex_array_cube_mVBO_normal.set_attribute_array(
				ctx, surface_shader.get_normal_index(), vec3type_mVBO_normal, vb_cube_mVBO_normal,
				0, // no offset
				vertices_cube_normal.size(), // number of normal elements in the array
				sizeof(cgv::vec3) // no stride for multi vbo
			) && success;
			// Set color.
			success = vertex_array_cube_mVBO_color.set_attribute_array(
				ctx, surface_shader.get_color_index(), vec4type_mVBO_color, vb_cube_mVBO_color,
				0, // no offset
				vertices_cube_color.size(), // number of color elements in the array
				sizeof(cgv::vec4) // No stride for multi vbo
			) && success;
		}
		else
		{
			ERROR("Unknown interleaved mode in Init.");
			// - obtain type descriptors for the automatic array binding facilities of the
			//   framework
			cgv::render::type_descriptor
				vec3type_pos =
				cgv::render::element_descriptor_traits<cgv::vec3>
				::get_type_descriptor(vertices_cube[0].pos),
				vec3type_normal =
				cgv::render::element_descriptor_traits<cgv::vec3>
				::get_type_descriptor(vertices_cube[0].normal),
				vec4type_color =
				cgv::render::element_descriptor_traits<cgv::vec4>
				::get_type_descriptor(vertices_cube[0].color);

			// - create buffer objects
			success = vb_cube.create(ctx, &(vertices_cube[0]), vertices_cube.size()) && success;
			success = vertex_array_cube.create(ctx) && success;

			// Set attribute array - Interleaved
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
				sizeof(cgv::vec3) * 2, // color vec follow after pos and mormal
				vertices_cube.size(), // number of color elements in the array
				sizeof(vertex_cube) // stride from one element to next
			) && success;
		}*/

		#ifndef Interleaved VBO
		// INTERLEAVED --- 
		// - obtain type descriptors for the automatic array binding facilities of the
		//   framework
		cgv::render::type_descriptor
		vec3type_pos =
		cgv::render::element_descriptor_traits<cgv::vec3>
		::get_type_descriptor(vertices_cube[0].pos),
		vec3type_normal =
		cgv::render::element_descriptor_traits<cgv::vec3>
		::get_type_descriptor(vertices_cube[0].normal),
		vec4type_color =
		cgv::render::element_descriptor_traits<cgv::vec4>
		::get_type_descriptor(vertices_cube[0].color);
		
		// - create buffer objects
		success = vb_cube.create(ctx, &(vertices_cube[0]), vertices_cube.size()) && success;
		success = vertex_array_cube.create(ctx) && success;
		
		// - Set attribute array - Interleaved
		// - Set position.
		success = vertex_array_cube.set_attribute_array(
			ctx, surface_shader.get_position_index(), vec3type_pos, vb_cube,
			0, // position is at start of the struct <-> offset = 0
			vertices_cube.size(), // number of position elements in the array
			sizeof(vertex_cube) // stride from one element to next
		) && success;
		// - Set normal.
		success = vertex_array_cube.set_attribute_array(
			ctx, surface_shader.get_normal_index(), vec3type_normal, vb_cube,
			sizeof(cgv::vec3), // normal coords follow after position
			vertices_cube.size(), // number of normal elements in the array
			sizeof(vertex_cube) // stride from one element to next
		) && success;
		// - Set color.
		success = vertex_array_cube.set_attribute_array(
			ctx, surface_shader.get_color_index(), vec4type_color, vb_cube,
			sizeof(cgv::vec3) * 2, // color vec follow after pos and mormal
			vertices_cube.size(), // number of color elements in the array
			sizeof(vertex_cube) // stride from one element to next
		) && success;

		#endif // !Interleaved VBO

		#ifndef Non-Interleaved Single VBO
		// SINGLE VBO ---
				// - non-interleaved single vbo.
		cgv::render::type_descriptor
			vec3type_sVBO_pos =
			cgv::render::element_descriptor_traits<cgv::vec3>
			::get_type_descriptor(vertices_sVBO[0]),
			vec3type_sVBO_normal =
			cgv::render::element_descriptor_traits<cgv::vec3>
			::get_type_descriptor(vertices_sVBO[vertices_cube_size]);

		// - create VBO.
		success = vb_cube_sVBO.create(ctx, &(vertices_sVBO[0]), vertices_sVBO.size()) && success;
		success = vertex_array_cube_sVBO.create(ctx) && success;

		// - Set attribute array - Interleaved - Single VBO
		// - Set position.
		success = vertex_array_cube_sVBO.set_attribute_array(
			ctx, surface_shader.get_position_index(), vec3type_sVBO_pos, vb_cube_sVBO,
			0, // no offset
			vertices_cube_pos.size(), // number of position elements in the array
			0 // no stride for multi vbo
		) && success;
		// - Set normal.
		success = vertex_array_cube_sVBO.set_attribute_array(
			ctx, surface_shader.get_normal_index(), vec3type_sVBO_normal, vb_cube_sVBO,
			sizeof(cgv::vec3) * vertices_cube_pos.size(), // no offset
			vertices_cube_normal.size(), // number of normal elements in the array
			0 // no stride for multi vbo
		) && success;
		#endif // !Non-Interleaved Single VBO

		#ifndef All in one VBO
		// ALL in ONE --- 
		// - obtain type descriptors for the automatic array binding facilities of the
		//   framework
		cgv::render::type_descriptor
			vec3type_AiO_pos =
			cgv::render::element_descriptor_traits<cgv::vec3>
			::get_type_descriptor(vertices_AiO[0].pos),
			vec3type_AiO_normal =
			cgv::render::element_descriptor_traits<cgv::vec3>
			::get_type_descriptor(vertices_AiO[0].normal),
			vec4type_AiO_color =
			cgv::render::element_descriptor_traits<cgv::vec4>
			::get_type_descriptor(vertices_AiO[0].color);

		// - create buffer objects
		success = vb_AiO.create(ctx, &(vertices_AiO[0]), vertices_AiO.size()) && success;
		success = va_AiO.create(ctx) && success;
		DEBUG("Init vertices_AiO.size(): " << vertices_AiO.size());

		// - Set attribute array - Interleaved
		// - Set position.
		success = va_AiO.set_attribute_array(
			ctx, surface_shader.get_position_index(), vec3type_AiO_pos, vb_AiO,
			0, // position is at start of the struct <-> offset = 0
			vertices_AiO.size(), // number of position elements in the array
			sizeof(vertex_cube) // stride from one element to next
		) && success;
		// - Set normal.
		success = va_AiO.set_attribute_array(
			ctx, surface_shader.get_normal_index(), vec3type_AiO_normal, vb_AiO,
			sizeof(cgv::vec3), // normal coords follow after position
			vertices_AiO.size(), // number of normal elements in the array
			sizeof(vertex_cube) // stride from one element to next
		) && success;
		// - Set color.
		success = va_AiO.set_attribute_array(
			ctx, surface_shader.get_color_index(), vec4type_AiO_color, vb_AiO,
			sizeof(cgv::vec3) * 2, // color vec follow after pos and mormal
			vertices_AiO.size(), // number of color elements in the array
			sizeof(vertex_cube) // stride from one element to next
		) && success;

		#endif // !All in one VBO


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
			DEBUG("Fb invalied !!!");
			vertices_AiO.clear();
			vertices_cube.clear();
			vertices_cube_color.clear();
			vertices_cube_normal.clear();
			vertices_cube_pos.clear();
			num_level.clear();
			init(ctx);

		}
	}

	// Should be overwritten to sensibly implement the cgv::render::drawable interface
	void draw(cgv::render::context& ctx)
	{
		// Test the start time of rendering.
		auto start = std::chrono::high_resolution_clock::now();
		INFO("Now in draw, start at " << start.time_since_epoch().count());

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

		draw_my_cube(ctx);

		// Restore the attributes.
		glPopAttrib();

		// Disable shader program
		default_shader.disable(ctx);

		auto end = std::chrono::high_resolution_clock::now();
		INFO("Now leaving draw, end at " << end.time_since_epoch().count());
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		INFO("Duration of rendering: " << duration.count() << " ms" << std::endl);
	}

	// Creates geometry for the cube
	void init_unit_cube_geometry(void)
	{
		// Prepare array
		vertices_cube.resize(vertices_cube_size);
		vertices_cube_pos.resize(vertices_cube_size);
		vertices_cube_normal.resize(vertices_cube_size);
		vertices_cube_color.resize(vertices_cube_size);

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

#ifndef cube_vertices_trianles
		
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
		//Front face
vertices_cube[0].pos = cgv::vec3(-1, -1, 1);
vertices_cube[1].pos = cgv::vec3(1, -1, 1);
vertices_cube[2].pos = cgv::vec3(-1, 1, 1);

vertices_cube[3].pos = cgv::vec3(1, 1, 1);
vertices_cube[4].pos = cgv::vec3(-1, 1, 1);
vertices_cube[5].pos = cgv::vec3(1, -1, 1);

vertices_cube[0].normal = cgv::vec3(0, 0, 1);
vertices_cube[1].normal = cgv::vec3(0, 0, 1);
vertices_cube[2].normal = cgv::vec3(0, 0, 1);

vertices_cube[3].normal = cgv::vec3(0, 0, 1);
vertices_cube[4].normal = cgv::vec3(0, 0, 1);
vertices_cube[5].normal = cgv::vec3(0, 0, 1);

//Back face
vertices_cube[6].pos = cgv::vec3(-1, -1, -1);
vertices_cube[7].pos = cgv::vec3(-1, 1, -1);
vertices_cube[8].pos = cgv::vec3(1, -1, -1);

vertices_cube[9].pos = cgv::vec3(1, -1, -1);
vertices_cube[10].pos = cgv::vec3(-1, 1, -1);
vertices_cube[11].pos = cgv::vec3(1, 1, -1);

vertices_cube[6].normal = cgv::vec3(0, 0, -1);
vertices_cube[7].normal = cgv::vec3(0, 0, -1);
vertices_cube[8].normal = cgv::vec3(0, 0, -1);

vertices_cube[9].normal = cgv::vec3(0, 0, -1);
vertices_cube[10].normal = cgv::vec3(0, 0, -1);
vertices_cube[11].normal = cgv::vec3(0, 0, -1);

//Left face
vertices_cube[12].pos = cgv::vec3(-1, -1, -1);
vertices_cube[13].pos = cgv::vec3(-1, -1, 1);
vertices_cube[14].pos = cgv::vec3(-1, 1, -1);

vertices_cube[15].pos = cgv::vec3(-1, 1, -1);
vertices_cube[16].pos = cgv::vec3(-1, -1, 1);
vertices_cube[17].pos = cgv::vec3(-1, 1, 1);

vertices_cube[12].normal = cgv::vec3(-1, 0, 0);
vertices_cube[13].normal = cgv::vec3(-1, 0, 0);
vertices_cube[14].normal = cgv::vec3(-1, 0, 0);

vertices_cube[15].normal = cgv::vec3(-1, 0, 0);
vertices_cube[16].normal = cgv::vec3(-1, 0, 0);
vertices_cube[17].normal = cgv::vec3(-1, 0, 0);

//Right face
vertices_cube[18].pos = cgv::vec3(1, -1, -1);
vertices_cube[19].pos = cgv::vec3(1, 1, -1);
vertices_cube[20].pos = cgv::vec3(1, -1, 1);

vertices_cube[21].pos = cgv::vec3(1, -1, 1);
vertices_cube[22].pos = cgv::vec3(1, 1, -1);
vertices_cube[23].pos = cgv::vec3(1, 1, 1);

vertices_cube[18].normal = cgv::vec3(1, 0, 0);
vertices_cube[19].normal = cgv::vec3(1, 0, 0);
vertices_cube[20].normal = cgv::vec3(1, 0, 0);

vertices_cube[21].normal = cgv::vec3(1, 0, 0);
vertices_cube[22].normal = cgv::vec3(1, 0, 0);
vertices_cube[23].normal = cgv::vec3(1, 0, 0);

//Top face
vertices_cube[24].pos = cgv::vec3(-1, 1, -1);
vertices_cube[25].pos = cgv::vec3(-1, 1, 1);
vertices_cube[26].pos = cgv::vec3(1, 1, -1);

vertices_cube[27].pos = cgv::vec3(1, 1, -1);
vertices_cube[28].pos = cgv::vec3(-1, 1, 1);
vertices_cube[29].pos = cgv::vec3(1, 1, 1);

vertices_cube[24].normal = cgv::vec3(0, 1, 0);
vertices_cube[25].normal = cgv::vec3(0, 1, 0);
vertices_cube[26].normal = cgv::vec3(0, 1, 0);

vertices_cube[27].normal = cgv::vec3(0, 1, 0);
vertices_cube[28].normal = cgv::vec3(0, 1, 0);
vertices_cube[29].normal = cgv::vec3(0, 1, 0);

//Bottom face
vertices_cube[30].pos = cgv::vec3(-1, -1, -1);
vertices_cube[31].pos = cgv::vec3(1, -1, -1);
vertices_cube[32].pos = cgv::vec3(-1, -1, 1);

vertices_cube[33].pos = cgv::vec3(1, -1, 1);
vertices_cube[34].pos = cgv::vec3(-1, -1, 1);
vertices_cube[35].pos = cgv::vec3(1, -1, -1);

vertices_cube[30].normal = cgv::vec3(0, -1, 0);
vertices_cube[31].normal = cgv::vec3(0, -1, 0);
vertices_cube[32].normal = cgv::vec3(0, -1, 0);

vertices_cube[33].normal = cgv::vec3(0, -1, 0);
vertices_cube[34].normal = cgv::vec3(0, -1, 0);
vertices_cube[35].normal = cgv::vec3(0, -1, 0);
*/
#endif

		for (int i = 0; i < vertices_cube.size(); i++)
		{
			vertices_cube_pos[i] = vertices_cube[i].pos;
			vertices_cube_normal[i] = vertices_cube[i].normal;
			vertices_cube_color[i] = vertices_cube[i].color;
		}

		vertices_sVBO.resize(vertices_cube_size * 2);
		for (int i = 0; i < vertices_cube.size(); i++)
		{
			vertices_sVBO[i] = vertices_cube_pos[i];
			vertices_sVBO[i + vertices_cube_size] = vertices_cube_normal[i];
		}
		

		init_all_in_one_geometry(cube_color, vertices_cube);

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


	// Calculate the number of cubes for all in one geometry.
	unsigned cal_num_cubes(unsigned max_depth)
	{
		unsigned num_level = 0; // number of present level cubes.
		unsigned last_level = 0;
		unsigned num_cubes = 0;
		// Calculate the number of cubes.
		for (unsigned i = 0; i <= max_depth; i++)
		{
			if (i < 2)
			{
				num_level = i == 0 ? 1 : 4;
				num_cubes += num_level;
			}
			else
			{
				num_level = (3 * num_level);
				num_cubes += num_level;
			}
		}
		return num_cubes;
	}
	// Recursive add cubes in all in one geometry
	void add_next_level_cubes(
		std::vector<cubes_drawable::vertex_cube> vertices_parent,
		unsigned level, unsigned max_depth, 
		cgv::dmat4 matrix, const cgv::media::color<float>& color_parent
	)
	{
		// Schrink the cube.
		if (level != 0)
		{
			matrix = matrix * cgv::math::scale4<double>(0.5, 0.5, 0.5);
		}

		// Set present level color
		cgv::media::color<float, cgv::media::HLS> color_next_parent(color_parent);
		color_next_parent.H() = std::fmod(color_next_parent.H() + 0.2f, 1.0f);
		color_next_parent.S() = std::fmod(color_next_parent.S() - 0.05f, 1.0f);

		unsigned num_children = level == 0 ? 4 : 3;
		if (level < max_depth)
		{
			//DEBUG("Level-" << level + 1 << " : num_children-" << num_children);
			//DEBUG("Level-" << level + 1 << " : now_child-" << num_level[level+1]);
			auto matrix_transform = matrix;
			for (unsigned i = 0; i < num_children; i++)
			{
				// Transform matrix
				matrix_transform =
					matrix *
					cgv::math::rotate4<double>(signed(i) * 90 - 90, 0, 0, 1.0) *
					cgv::math::translate4<double>(2, 0, 0);

				// Vertices for this generated cube.
				std::vector<cubes_drawable::vertex_cube> vertices_next_parent;
				vertices_next_parent.resize(vertices_cube_size);

				for (unsigned j = 0; j < vertices_cube_size; j++)
				{
					vertices_next_parent[j].pos = matrix_transform.mul_pos(vertices_AiO[j].pos);
					vertices_next_parent[j].normal = matrix_transform.mul_dir(vertices_AiO[j].normal);
					vertices_next_parent[j].color = cgv::vec4(color_next_parent.R(), color_next_parent.G(), color_next_parent.B(), 1.0f);

					//DEBUG("matrix_transform.mul_pos(vertices_AiO[j].pos):" << matrix_transform.mul_pos(vertices_AiO[j].pos) << " - vertices_AiO[j].pos:"<< vertices_AiO[j].pos);
					// Index of this cube: (num_cubes + i) * 36
					vertices_AiO[(cal_num_cubes(level) + num_level[level + 1]) * 36 + j].pos = vertices_next_parent[j].pos;
					vertices_AiO[(cal_num_cubes(level) + num_level[level + 1]) * 36 + j].normal = vertices_next_parent[j].normal;
					vertices_AiO[(cal_num_cubes(level) + num_level[level + 1]) * 36 + j].color = vertices_next_parent[j].color;
				}
				num_level[level+1] = num_level[level+1] + 1;


				//DEBUG("Level-" << level+1 << " : ++now_child-" << num_level[level + 1]);
				//DEBUG("Index of this cube: " << (cal_num_cubes(level) + num_level[level + 1]-1) * 36 );
				add_next_level_cubes(vertices_next_parent, level+1, max_depth, matrix_transform, color_next_parent);
				vertices_next_parent.clear();
				// Need to call add_next_level_cubes(...) for every cube in last level.
			}
		}

	}
	// Init the geometry for all_in_one cube.
	void init_all_in_one_geometry(
		const cgv::media::color<float>& color, 
		std::vector<cubes_drawable::vertex_cube> vertices_cube)
	{
		// init num_level
		num_level.resize(recursion_depth+1);
		num_level[0] = 1;
		for (int i = 1; i < recursion_depth; i++)
		{
			num_level[i] = 0;
		}

		// Matrix for schrink the present level cubes.
		cgv::dmat4 matrix = cgv::math::scale4<double>(0.5, 0.5, 0.5);

		// Set present cube as the parent cube for next level.
		std::vector<cubes_drawable::vertex_cube> vertices_next_parent;
		vertices_next_parent.resize(vertices_cube_size);
		// First unit cube
		vertices_AiO.resize(cal_num_cubes(recursion_depth) * vertices_cube_size);
		DEBUG("vertices_AiO size: " << vertices_AiO.size());
		DEBUG("vertices_cube_size: " << vertices_cube_size);
		DEBUG("vertices_next_parent size: " << vertices_next_parent.size());
		for (unsigned i=0; i<vertices_cube_size; i++)
		{
			vertices_next_parent[i].pos = matrix.mul_pos(vertices_cube[i].pos);
			vertices_next_parent[i].normal = matrix.mul_dir(vertices_cube[i].normal);
			vertices_next_parent[i].color = cgv::vec4(color.R(), color.G(), color.B(), 1.0f); 
			vertices_AiO[i] = {vertices_next_parent[i].pos, vertices_next_parent[i].normal, vertices_next_parent[i].color};
		}

		// Call func for init next level cubes
		cgv::media::color<float, cgv::media::HLS> color_next_parent(color);
		add_next_level_cubes(vertices_next_parent, 0, recursion_depth, matrix, color_next_parent);
		vertices_next_parent.clear();

		DEBUG("vertices_cube_size: " << vertices_cube_size);
		DEBUG("recursion_depth: " << recursion_depth);
		DEBUG("vertices_AiO.size(): " << vertices_AiO.size());
	}

	/*
	void draw_all_in_one(cgv::render::context& ctx, size_t va_num_verts, const cgv::media::color<float>& color, unsigned max_depth,
		unsigned level)
	{
		// Shrink by factor of 1/2 relative to parent node
		ctx.mul_modelview_matrix(cgv::math::scale4<double>(0.5, 0.5, 0.5));

		// Render the cube
		material.set_diffuse_reflectance(color);
		ctx.set_material(material);

		// Recurse
		unsigned num_children = level == 0 ? 4 : 3;
		if (level < max_depth)
		{
			for (unsigned i = 0; i < num_children; i++)
			{
				// Save transformation before recursion
				ctx.push_modelview_matrix();
				// Transform to node position
				ctx.mul_modelview_matrix(
					// rotate around z-axis by -90, 0, 90 or 180 degrees
					cgv::math::rotate4<double>(signed(i) * 90 - 90, 0, 0, 1)
					// move along x-axis by 2 relative units
					* cgv::math::translate4<double>(2, 0, 0)
				);
				// Actual recursion
				draw_recursive(ctx, color_next, max_depth, level + 1);
				// Restore pre-recursion transformation
				ctx.pop_modelview_matrix();
			}
		}

		if (va_all_in_one)
		{
			// Use the vertex array
			va_all_in_one->enable(ctx);
			glDrawArrays(GL_Primitive, 0, (GLsizei)va_num_verts);
			va_all_in_one->disable(ctx);
		}
		else
		{
			// Use built-in unit quad
			ctx.tesselate_unit_cube();
		}
		cgv::media::color<float, cgv::media::HLS> color_next(color);
		color_next.H() = std::fmod(color_next.H() + 0.2f, 1.0f);
		color_next.S() = std::fmod(color_next.S() - 0.05f, 1.0f);

		// Recurse
		unsigned num_children = level == 0 ? 4 : 3;
		if (level < max_depth) 
		{
			for (unsigned i = 0; i < num_children; i++)
			{
				// Save transformation before recursion
				ctx.push_modelview_matrix();
				// Transform to node position
				ctx.mul_modelview_matrix(
					// rotate around z-axis by -90, 0, 90 or 180 degrees
					cgv::math::rotate4<double>(signed(i) * 90 - 90, 0, 0, 1)
					// move along x-axis by 2 relative units
					* cgv::math::translate4<double>(2, 0, 0)
				);
				// Actual recursion
				draw_recursive(ctx, color_next, max_depth, level + 1);
				// Restore pre-recursion transformation
				ctx.pop_modelview_matrix();
			}
		}
	}
	*/

	// Draw method for a custom quad
	void draw_my_cube(cgv::render::context& ctx)
	{
		// Switch the GL_Primitive mode.
		if(primitive_mode == QUADS)
		{
			INFO("GL_Primitive -> GL_QUADS");
			GL_Primitive = GL_QUADS;
		}
		else if(primitive_mode == TRIANGLES)
		{
			INFO("GL_Primitive -> GL_TRIANGLES");
			GL_Primitive = GL_TRIANGLES;
		}
		else if (primitive_mode == TRIANGLE_STRIP)
		{
			INFO("GL_Primitive -> GL_TRIANGLE_STRIP");
			GL_Primitive = GL_TRIANGLE_STRIP;
		}
		else
		{
			INFO("Default GL_Primitive -> GL_QUADS");
			GL_Primitive = GL_QUADS;
		}
		
		// Switch the interleaved mode.
		if (interleaved_mode == INTERLEAVED)
		{
			INFO("interleaved_mode -> INTERLEAVED.");
			fractal->use_vertex_array(&vertex_array_cube, vertices_cube.size(), GL_Primitive);
		}
		else if (interleaved_mode == NON_INTERLEAVED_SINGLE_VBO)
		{
			INFO("interleaved_mode -> SINGLE_VBO.");
			fractal->use_vertex_array(&vertex_array_cube_sVBO, vertices_cube.size(), GL_Primitive);
		}
		/*else if (interleaved_mode == MULTI_VBO)
		{
			DEBUG("interleaved_mode -> MULTI_VBO: " << interleaved_mode);
			fractal->use_vertex_array(&vertex_array_cube_mVBO_pos, vertices_cube.size(), GL_Primitive);
			fractal->use_vertex_array(&vertex_array_cube_mVBO_normal, vertices_cube.size(), GL_Primitive);
			fractal->use_vertex_array(&vertex_array_cube_mVBO_color, vertices_cube.size(), GL_Primitive);
		}*/
		else if (interleaved_mode == BUILTIN)
		{
			INFO("interleaved_mode -> BUILT_IN.");
			fractal->use_vertex_array(nullptr, 0, GL_Primitive);
		}
		else if (interleaved_mode == ALL_in_ONE)
		{
			INFO("default interleaved_mode -> ALL_in_ONE.");
		}
		else
		{
			interleaved_mode = INTERLEAVED;
			INFO("default interleaved_mode -> INTERLEAVED.");
			fractal->use_vertex_array(&vertex_array_cube, vertices_cube.size(), GL_Primitive);
		}
				
		if (interleaved_mode != ALL_in_ONE)
		{
			INFO("Draw Unit Cube");
			fractal->draw_recursive(ctx, cube_color, recursion_depth);
		}
		else
		{
			material.set_diffuse_reflectance(cube_color);
			ctx.set_material(material);
			INFO("Draw AiO Cube");
			va_AiO.enable(ctx);
			glDrawArrays(GL_Primitive, 0, vertices_AiO.size());
			va_AiO.disable(ctx);
		}
	}
};

// [END] Tasks 1.2a, 1.2b and 1.2c
// ************************************************************************************/


// ************************************************************************************/
// Task 1.2a: register an instance of your drawable.

// Create an instance of the demo class at plugin load and register it with the framework
cgv::base::object_registration<cubes_drawable> cubes_drawable_registration("cubes_drawable");
