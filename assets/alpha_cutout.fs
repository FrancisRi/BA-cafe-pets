#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform int alphaMode; // glTF: 0 = OPAQUE, 1 = MASK, 2 = BLEND
uniform float alphaCutoff;
out vec4 finalColor;
void main()
{
    vec4 color = texture(texture0, fragTexCoord)*colDiffuse*fragColor;
    if (alphaMode == 1 && color.a < alphaCutoff) discard;
    if (alphaMode != 2) color.a = 1.0;
    finalColor = color;
}
