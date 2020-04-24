#include <glad/glad.h>
#include <wglext.h>

#include "handmade_renderer.h"
#include "win32_handmade_opengl.h"

#define GladLoadGLLoader gladLoadGLLoader
#define GladSetPostCallback glad_set_post_callback

void GladPostCallback(const char *Name, void *FuncPtr, i32 LenArgs, ...) {
	GLenum ErrorCode;
	ErrorCode = glad_glGetError();

	if (ErrorCode != GL_NO_ERROR)
	{
		char OpenGLError[256];
		FormatString(OpenGLError, ArrayCount(OpenGLError), "OpenGL Error: %d in %s\n", ErrorCode, Name);

		Win32OutputString(OpenGLError);
	}
}

inline void *
GetOpenGLFuncAddress(char *Name)
{
	// https://www.khronos.org/opengl/wiki/Load_OpenGL_Functions

    void *Result = (void *)wglGetProcAddress(Name);
    if (Result == 0 || (Result == (void *)0x1) || (Result == (void *)0x2) || (Result == (void *)0x3) || (Result == (void *)-1))
    {
        HMODULE OpenGLModule = LoadLibraryA("opengl32.dll");
        if (OpenGLModule)
        {
            Result = (void *)GetProcAddress(OpenGLModule, Name);
        }
    }

    return Result;
}

inline void
Win32OpenGLSetVSync(opengl_state *State, b32 VSync)
{
	State->wglSwapIntervalEXT(VSync);
}

void internal
Win32InitOpenGL(opengl_state *State, HINSTANCE hInstance, HWND WindowHandle)
{
    WNDCLASS FakeWindowClass = {};
    FakeWindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    FakeWindowClass.lpfnWndProc = DefWindowProc;
    FakeWindowClass.hInstance = hInstance;
    FakeWindowClass.lpszClassName = L"Fake OpenGL Window Class";

    RegisterClass(&FakeWindowClass);

    HWND FakeWindowHandle = CreateWindowEx(0, FakeWindowClass.lpszClassName, L"Fake OpenGL Window", 0,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0
	);
    HDC FakeWindowDC = GetDC(FakeWindowHandle);

    PIXELFORMATDESCRIPTOR FakePFD = {};
    FakePFD.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    FakePFD.nVersion = 1;
    FakePFD.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    FakePFD.iPixelType = PFD_TYPE_RGBA;
    FakePFD.cColorBits = 32;
    FakePFD.cAlphaBits = 8;
    FakePFD.cDepthBits = 24;

    i32 FakePixelFormatIndex = ChoosePixelFormat(FakeWindowDC, &FakePFD);

    if (FakePixelFormatIndex)
    {
        if (SetPixelFormat(FakeWindowDC, FakePixelFormatIndex, &FakePFD)) {
            HGLRC FakeRC = wglCreateContext(FakeWindowDC);

            if (wglMakeCurrent(FakeWindowDC, FakeRC))
            {
                // OpenGL 1.1 is initialized

                HDC WindowDC = GetDC(WindowHandle);

                PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
                PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
                State->wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
				State->wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");

                i32 PixelAttribs[] = {
                    WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
                    WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
                    WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
                    WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
                    WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
                    WGL_COLOR_BITS_ARB, 32,
                    WGL_ALPHA_BITS_ARB, 8,
                    WGL_DEPTH_BITS_ARB, 24,
                    WGL_STENCIL_BITS_ARB, 8,
                    WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
                    WGL_SAMPLES_ARB, 0,
                    0
                };

                i32 PixelFormatIndex;
                u32 NumFormats;
                wglChoosePixelFormatARB(WindowDC, PixelAttribs, 0, 1, &PixelFormatIndex, &NumFormats);

                PIXELFORMATDESCRIPTOR PFD = {};
                DescribePixelFormat(WindowDC, PixelFormatIndex, sizeof(PFD), &PFD);
                
                if (SetPixelFormat(WindowDC, PixelFormatIndex, &PFD))
                {
                    i32 OpenGLVersionMajor = 4;
                    i32 OpenGLVersionMinor = 5;

                    i32 ContextAttribs[] = {
                        WGL_CONTEXT_MAJOR_VERSION_ARB, OpenGLVersionMajor,
                        WGL_CONTEXT_MINOR_VERSION_ARB, OpenGLVersionMinor,
                        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                        0
                    };

                    HGLRC RC = wglCreateContextAttribsARB(WindowDC, 0, ContextAttribs);

                    wglMakeCurrent(0, 0);
                    wglDeleteContext(FakeRC);
                    ReleaseDC(FakeWindowHandle, FakeWindowDC);
                    DestroyWindow(FakeWindowHandle);

                    if (wglMakeCurrent(WindowDC, RC))
                    {
                        // OpenGL 4.5 is initialized
                        GladLoadGLLoader((GLADloadproc)GetOpenGLFuncAddress);
						GladSetPostCallback(GladPostCallback);

                        State->Vendor = (char *)glGetString(GL_VENDOR);
                        State->Renderer = (char *)glGetString(GL_RENDERER);
                        State->Version = (char *)glGetString(GL_VERSION);
                        State->ShadingLanguageVersion = (char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
                    }
                }
            }
        }
    }
}

internal GLuint
CreateShader(GLenum Type, char *Source)
{
	GLuint Shader = glCreateShader(Type);
	glShaderSource(Shader, 1, &Source, NULL);
	glCompileShader(Shader);

	i32 IsShaderCompiled;
	glGetShaderiv(Shader, GL_COMPILE_STATUS, &IsShaderCompiled);
	if (!IsShaderCompiled)
	{
		i32 LogLength;
		glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &LogLength);

		char ErrorLog[256];
		glGetShaderInfoLog(Shader, LogLength, NULL, ErrorLog);
		
		//PrintError("Error: ", "Shader compilation failed", ErrorLog);

		glDeleteShader(Shader);
	}
	Assert(IsShaderCompiled);

	return Shader;
}

internal GLuint
CreateProgram(GLuint VertexShader, GLuint FragmentShader)
{
	GLuint Program = glCreateProgram();
	glAttachShader(Program, VertexShader);
	glAttachShader(Program, FragmentShader);
	glLinkProgram(Program);
	glDeleteShader(VertexShader);
	glDeleteShader(FragmentShader);

	i32 IsProgramLinked;
	glGetProgramiv(Program, GL_LINK_STATUS, &IsProgramLinked);

	if (!IsProgramLinked)
	{
		i32 LogLength;
		glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &LogLength);

		char ErrorLog[256];
		glGetProgramInfoLog(Program, LogLength, nullptr, ErrorLog);

		//PrintError("Error: ", "Shader program linkage failed", ErrorLog);
	}
	Assert(IsProgramLinked);

	return Program;
}

char *SimpleVertexShader = (char *)
R"SHADER(
#version 450

layout(location = 0) in vec3 in_Position;

uniform mat4 u_Projection;
uniform mat4 u_View;
uniform mat4 u_Model;

void main()
{ 
    gl_Position = u_Projection * u_View * u_Model * vec4(in_Position, 1.f);
}
)SHADER";

char *SimpleFragmentShader = (char *)
R"SHADER(
#version 450

out vec4 out_Color;

uniform vec4 u_Color;

void main()
{
	out_Color = u_Color;
}
)SHADER";

char *GridVertexShader = (char *)
R"SHADER(
#version 450

layout(location = 0) in vec3 in_Position;

out vec3 ex_VertexPosition; 

uniform mat4 u_Projection;
uniform mat4 u_View;
uniform mat4 u_Model;

void main()
{
    ex_VertexPosition = (u_Model * vec4(in_Position, 1.f)).xyz;
    gl_Position = u_Projection * u_View * u_Model * vec4(in_Position, 1.f);
}
)SHADER";

char *GridFragmentShader = (char *)
R"SHADER(
#version 450

in vec3 ex_VertexPosition;

out vec4 out_Color;

uniform vec3 u_CameraPosition;
uniform vec3 u_Color;

void main()
{
    float DistanceFromCamera = length(u_CameraPosition - ex_VertexPosition);
    float Opacity = clamp(DistanceFromCamera * 0.01f, 0.f, 1.f);

    out_Color = vec4(u_Color, 1.f - Opacity);
}
)SHADER";

internal void
OpenGLProcessRenderCommands(opengl_state *State, render_commands *Commands)
{
    for (u32 BaseAddress = 0; BaseAddress < Commands->RenderCommandsBufferSize;)
    {
        render_command_header *Entry = (render_command_header *)((u8 *)Commands->RenderCommandsBuffer + BaseAddress);

        switch (Entry->Type)
        {
			case RenderCommand_SetViewport:
			{
				// todo:
				glEnable(GL_CULL_FACE);
				//glEnable(GL_DEPTH_TEST);
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glEnable(GL_LINE_SMOOTH);
				glEnable(GL_MULTISAMPLE);

				render_command_set_viewport *Command = (render_command_set_viewport *)Entry;

				glViewport(Command->x, Command->y, Command->Width, Command->Height);

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_SetOrthographicProjection:
			{
				render_command_set_orthographic_projection *Command = (render_command_set_orthographic_projection *)Entry;

				// todo: use uniform buffer
				glUseProgram(State->SimpleShaderProgram);
				{
					mat4 Projection = Orthographic(Command->Left, Command->Right, Command->Bottom, Command->Top, Command->Near, Command->Far);

					i32 ProjectionUniformLocation = glGetUniformLocation(State->SimpleShaderProgram, "u_Projection");
					glUniformMatrix4fv(ProjectionUniformLocation, 1, GL_TRUE, &Projection.Elements[0][0]);
				}
				glUseProgram(0);

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_SetPerspectiveProjection:
			{
				render_command_set_perspective_projection *Command = (render_command_set_perspective_projection *)Entry;

				// todo: use uniform buffer
				glUseProgram(State->SimpleShaderProgram);
				{
					mat4 Projection = Perspective(Command->FovY, Command->Aspect, Command->Near, Command->Far);

					i32 ProjectionUniformLocation = glGetUniformLocation(State->SimpleShaderProgram, "u_Projection");
					glUniformMatrix4fv(ProjectionUniformLocation, 1, GL_TRUE, &Projection.Elements[0][0]);
				}
				glUseProgram(0);

				glUseProgram(State->GridShaderProgram);
				{
					mat4 Projection = Perspective(Command->FovY, Command->Aspect, Command->Near, Command->Far);

					i32 ProjectionUniformLocation = glGetUniformLocation(State->GridShaderProgram, "u_Projection");
					glUniformMatrix4fv(ProjectionUniformLocation, 1, GL_TRUE, &Projection.Elements[0][0]);
				}
				glUseProgram(0);

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_SetCameraTransform:
			{
				render_command_set_camera_transform *Command = (render_command_set_camera_transform *)Entry;

				// todo: use uniform buffer
				glUseProgram(State->SimpleShaderProgram);
				{
					mat4 View = LookAtLH(Command->Eye, Command->Target, Command->Up);

					i32 ViewUniformLocation = glGetUniformLocation(State->SimpleShaderProgram, "u_View");
					glUniformMatrix4fv(ViewUniformLocation, 1, GL_TRUE, &View.Elements[0][0]);
				}
				glUseProgram(0);

				glUseProgram(State->GridShaderProgram);
				{
					mat4 View = LookAtLH(Command->Eye, Command->Target, Command->Up);

					i32 ViewUniformLocation = glGetUniformLocation(State->GridShaderProgram, "u_View");
					glUniformMatrix4fv(ViewUniformLocation, 1, GL_TRUE, &View.Elements[0][0]);
				}
				glUseProgram(0);

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_SetWireframe:
			{
				render_command_set_wireframe *Command = (render_command_set_wireframe *)Entry;

				if (Command->IsWireframe)
				{
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				}
				else
				{
					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				}

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_Clear:
			{
				render_command_clear *Command = (render_command_clear *)Entry;

				glClearColor(Command->Color.x, Command->Color.y, Command->Color.z, Command->Color.w);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_InitLine:
			{
				render_command_init_line *Command = (render_command_init_line *)Entry;

				vec3 LineVertices[] = {
					vec3(0.f, 0.f, 0.f),
					vec3(1.f, 1.f, 1.f),
				};

				glGenVertexArrays(1, &State->LineVAO);
				glBindVertexArray(State->LineVAO);

				GLuint LineVBO;
				glGenBuffers(1, &LineVBO);
				glBindBuffer(GL_ARRAY_BUFFER, LineVBO);
				glBufferData(GL_ARRAY_BUFFER, sizeof(LineVertices), LineVertices, GL_STATIC_DRAW);

				glEnableVertexAttribArray(0);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), 0);

				glBindVertexArray(0);

				GLuint VertexShader = CreateShader(GL_VERTEX_SHADER, SimpleVertexShader);
				GLuint FragmentShader = CreateShader(GL_FRAGMENT_SHADER, SimpleFragmentShader);

				//State->SimpleShaderProgram = CreateProgram(VertexShader, FragmentShader);

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_DrawLine:
			{
				render_command_draw_line *Command = (render_command_draw_line *)Entry;

				// todo: restore line width
				glLineWidth(Command->Thickness);

				glBindVertexArray(State->LineVAO);
				glUseProgram(State->SimpleShaderProgram);
				{
					mat4 T = Translate(Command->Start);
					mat4 S = Scale(Command->End - Command->Start);

					mat4 Model = T * S;
					i32 ModelUniformLocation = glGetUniformLocation(State->SimpleShaderProgram, "u_Model");
					glUniformMatrix4fv(ModelUniformLocation, 1, GL_TRUE, (f32 *)Model.Elements);

					i32 ColorUniformLocation = glGetUniformLocation(State->SimpleShaderProgram, "u_Color");
					glUniform4f(ColorUniformLocation, Command->Color.r, Command->Color.g, Command->Color.b, Command->Color.a);
				}
				glDrawArrays(GL_LINES, 0, 2);

				glUseProgram(0);
				glBindVertexArray(0);

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_InitRectangle:
			{
				render_command_init_rectangle *Command = (render_command_init_rectangle *)Entry;

				vec3 RectangleVertices[] = {
					vec3(-1.f, -1.f, 0.f),
					vec3(1.f, -1.f, 0.f),
					vec3(-1.f, 1.f, 0.f),
					vec3(1.f, 1.f, 0.f)
				};

				glGenVertexArrays(1, &State->RectangleVAO);
				glBindVertexArray(State->RectangleVAO);

				GLuint RectangleVBO;
				glGenBuffers(1, &RectangleVBO);
				glBindBuffer(GL_ARRAY_BUFFER, RectangleVBO);
				glBufferData(GL_ARRAY_BUFFER, sizeof(RectangleVertices), RectangleVertices, GL_STATIC_DRAW);

				glEnableVertexAttribArray(0);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), 0);

				glBindVertexArray(0);

				GLuint VertexShader = CreateShader(GL_VERTEX_SHADER, SimpleVertexShader);
				GLuint FragmentShader = CreateShader(GL_FRAGMENT_SHADER, SimpleFragmentShader);

				State->SimpleShaderProgram = CreateProgram(VertexShader, FragmentShader);

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_DrawRectangle:
			{
				render_command_draw_rectangle *Command = (render_command_draw_rectangle *)Entry;

				glBindVertexArray(State->RectangleVAO);
				glUseProgram(State->SimpleShaderProgram);

				{
					mat4 T = Translate(vec3(Command->Position, 0.f));
					mat4 R = mat4(1.f);

					f32 Angle = Command->Rotation.x;
					vec3 Axis = Command->Rotation.yzw;

					if (IsXAxis(Axis))
					{
						R = RotateX(Angle);
					}
					else if (IsYAxis(Axis))
					{
						R = RotateY(Angle);
					}
					else if (IsZAxis(Axis))
					{
						R = RotateZ(Angle);
					}
					else
					{
						// todo:
					}

					mat4 S = Scale(Command->Size.x);

					mat4 Model = T * R * S;
					i32 ModelUniformLocation = glGetUniformLocation(State->SimpleShaderProgram, "u_Model");
					glUniformMatrix4fv(ModelUniformLocation, 1, GL_TRUE, (f32 *)Model.Elements);

#if 1
					mat4 View = mat4(1.f);
					i32 ViewUniformLocation = glGetUniformLocation(State->SimpleShaderProgram, "u_View");
					glUniformMatrix4fv(ViewUniformLocation, 1, GL_TRUE, &View.Elements[0][0]);
#endif

					i32 ColorUniformLocation = glGetUniformLocation(State->SimpleShaderProgram, "u_Color");
					glUniform4f(ColorUniformLocation, Command->Color.r, Command->Color.g, Command->Color.b, Command->Color.a);
				}
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			
				glUseProgram(0);
				glBindVertexArray(0);

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_InitBox:
			{
				render_command_init_box *Command = (render_command_init_box *)Entry;

				vec3 BoxVertices[] = {
					vec3(-1.f, 1.f, 1.f),		// Front-top-left
					vec3(1.f, 1.f, 1.f),		// Front-top-right
					vec3(-1.f, -1.f, 1.f),		// Front-bottom-left
					vec3(1.f, -1.f, 1.f),		// Front-bottom-right
					vec3(1.f, -1.f, -1.f),		// Back-bottom-right
					vec3(1.f, 1.f, 1.f),		// Front-top-right
					vec3(1.f, 1.f, -1.f),		// Back-top-right
					vec3(-1.f, 1.f, 1.f),		// Front-top-left
					vec3(-1.f, 1.f, -1.f),		// Back-top-left
					vec3(-1.f, -1.f, 1.f),		// Front-bottom-left
					vec3(-1.f, -1.f, -1.f),		// Back-bottom-left
					vec3(1.f, -1.f, -1.f),		// Back-bottom-right
					vec3(-1.f, 1.f, -1.f),		// Back-top-left
					vec3(1.f, 1.f, -1.f)		// Back-top-right
				};

				glGenVertexArrays(1, &State->BoxVAO);
				glBindVertexArray(State->BoxVAO);

				GLuint BoxVBO;
				glGenBuffers(1, &BoxVBO);
				glBindBuffer(GL_ARRAY_BUFFER, BoxVBO);
				glBufferData(GL_ARRAY_BUFFER, sizeof(BoxVertices), BoxVertices, GL_STATIC_DRAW);

				glEnableVertexAttribArray(0);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), 0);

				glBindVertexArray(0);

				GLuint VertexShader = CreateShader(GL_VERTEX_SHADER, SimpleVertexShader);
				GLuint FragmentShader = CreateShader(GL_FRAGMENT_SHADER, SimpleFragmentShader);

				//State->SimpleShaderProgram = CreateProgram(VertexShader, FragmentShader);

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_DrawBox:
			{
				render_command_draw_box *Command = (render_command_draw_box *)Entry;

				glBindVertexArray(State->BoxVAO);
				glUseProgram(State->SimpleShaderProgram);
				{
					mat4 T = Translate(Command->Position);
					mat4 R = mat4(1.f);
				
					f32 Angle = Command->Rotation.x;
					vec3 Axis = Command->Rotation.yzw;

					if (IsXAxis(Axis))
					{
						R = RotateX(Angle);
					}
					else if (IsYAxis(Axis))
					{
						R = RotateY(Angle);
					}
					else if (IsZAxis(Axis))
					{
						R = RotateZ(Angle);
					}
					else
					{
						// todo:
					}

					mat4 S = Scale(Command->Size);

					mat4 Model = T * R * S;

					i32 ModelUniformLocation = glGetUniformLocation(State->SimpleShaderProgram, "u_Model");
					glUniformMatrix4fv(ModelUniformLocation, 1, GL_TRUE, (f32 *)Model.Elements);

					i32 ColorUniformLocation = glGetUniformLocation(State->SimpleShaderProgram, "u_Color");
					glUniform4f(ColorUniformLocation, Command->Color.r, Command->Color.g, Command->Color.b, Command->Color.a);
				}
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 14);

				glUseProgram(0);
				glBindVertexArray(0);

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_InitGrid:
			{
				render_command_init_grid *Command = (render_command_init_grid *)Entry;

				temporary_memory TempMemory(&State->Arena);

				vec3 *GridVertices = PushArray(TempMemory.Arena, Command->Count * 8, vec3);

				for (u32 Index = 0; Index < Command->Count; ++Index)
				{
					f32 Coord = (f32)(Index + 1) / (f32)Command->Count;
					u32 GridVertexIndex = Index * 8;

					vec3 *GridVertex0 = GridVertices + GridVertexIndex + 0;
					*GridVertex0 = vec3(-Coord, 0.f, -1.f);

					vec3 *GridVertex1 = GridVertices + GridVertexIndex + 1;
					*GridVertex1 = vec3(-Coord, 0.f, 1.f);

					vec3 *GridVertex2 = GridVertices + GridVertexIndex + 2;
					*GridVertex2 = vec3(Coord, 0.f, -1.f);

					vec3 *GridVertex3 = GridVertices + GridVertexIndex + 3;
					*GridVertex3 = vec3(Coord, 0.f, 1.f);
					
					vec3 *GridVertex4 = GridVertices + GridVertexIndex + 4;
					*GridVertex4 = vec3(-1.f, 0.f, -Coord);

					vec3 *GridVertex5 = GridVertices + GridVertexIndex + 5;
					*GridVertex5 = vec3(1.f, 0.f, -Coord);

					vec3 *GridVertex6 = GridVertices + GridVertexIndex + 6;
					*GridVertex6 = vec3(-1.f, 0.f, Coord);

					vec3 *GridVertex7 = GridVertices + GridVertexIndex + 7;
					*GridVertex7 = vec3(1.f, 0.f, Coord);
				}

				glGenVertexArrays(1, &State->GridVAO);
				glBindVertexArray(State->GridVAO);

				GLuint GridVBO;
				glGenBuffers(1, &GridVBO);
				glBindBuffer(GL_ARRAY_BUFFER, GridVBO);
				glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) *Command->Count * 8, GridVertices, GL_STATIC_DRAW);

				glEnableVertexAttribArray(0);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), 0);

				glBindVertexArray(0);

				GLuint VertexShader = CreateShader(GL_VERTEX_SHADER, GridVertexShader);
				GLuint FragmentShader = CreateShader(GL_FRAGMENT_SHADER, GridFragmentShader);

				State->GridShaderProgram = CreateProgram(VertexShader, FragmentShader);

				BaseAddress += sizeof(*Command);
				break;
			}
			case RenderCommand_DrawGrid:
			{
				render_command_draw_grid *Command = (render_command_draw_grid *)Entry;

				glBindVertexArray(State->GridVAO);
				glUseProgram(State->GridShaderProgram);
				{
					mat4 Model = Scale(Command->Size);

					i32 ModelUniformLocation = glGetUniformLocation(State->GridShaderProgram, "u_Model");
					glUniformMatrix4fv(ModelUniformLocation, 1, GL_TRUE, (f32 *)Model.Elements);

					i32 ColorUniformLocation = glGetUniformLocation(State->GridShaderProgram, "u_Color");
					glUniform3f(ColorUniformLocation, Command->Color.r, Command->Color.g, Command->Color.b);

					i32 CameraPositionUniformLocation = glGetUniformLocation(State->GridShaderProgram, "u_CameraPosition");
					glUniform3f(CameraPositionUniformLocation, Command->CameraPosition.x, Command->CameraPosition.y, Command->CameraPosition.z);
				}
				glDrawArrays(GL_LINES, 0, Command->Count * 8);

				glUseProgram(0);
				glBindVertexArray(0);

				BaseAddress += sizeof(*Command);
				break;
			}
			default:
				Assert(!"Render command is not supported");
			}
    }
}
