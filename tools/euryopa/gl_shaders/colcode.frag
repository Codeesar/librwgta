#version 330

layout(std140) uniform State
{
	int   u_alphaTest;
	float u_alphaRef;

	int   u_fogEnable;
	float u_fogStart;
	float u_fogEnd;
	vec4  u_fogColor;
};

uniform sampler2D tex;

in vec4 v_color;
in vec2 v_tex0;

out vec4 color;

void
main(void)
{
	color = v_color;
	color.a *= texture(tex, vec2(v_tex0.x, v_tex0.y)).a;
	switch(u_alphaTest){
	default:
	case 0: break;
	case 1:
		if(color.a < u_alphaRef)
			discard;
		break;
	case 2:
		if(color.a >= u_alphaRef)
			discard;
		break;
	}
}

