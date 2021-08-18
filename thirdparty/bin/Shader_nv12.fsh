#version 330 core

in vec2 textureOut;

// Texture samplers
uniform sampler2D ourTextureY;
uniform sampler2D ourTextureUV;

void main()
{
        vec3 yuv;
        vec3 rgb;
        yuv.x = texture(ourTextureY, textureOut).r -16./256.;
    yuv.y = texture(ourTextureUV, textureOut).r - 128./256.;
        yuv.z = texture(ourTextureUV, textureOut).g - 128./256.;
        rgb = mat3( 1,       1,         1,
                    0,       -0.39465,  2.03211,
                    1.13983, -0.58060,  0) * yuv;
        gl_FragColor = vec4(rgb, 1);
}

