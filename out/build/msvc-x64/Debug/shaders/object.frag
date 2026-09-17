#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 UV;

out vec4 FragColor;

struct Light {
    vec3 position;
    vec3 color;
    float intensity;
};

#define MAX_DISCO_LIGHTS 4

uniform Light uLight1;       // glowne zrodlo swiatla (wymagane)
uniform Light uLight2;       // drugie zrodlo swiatla (wymagane)
uniform Light uDiscoLights[MAX_DISCO_LIGHTS];
uniform int uDiscoLightCount;
uniform bool uDiscoMode;

uniform vec3 uViewPos;
uniform sampler2D uTexture;
uniform bool uUseTexture;
uniform vec3 uBaseColor;
uniform float uShininess;

vec3 ComputeLight(Light light, vec3 normal, vec3 viewDir, vec3 albedo) {
    vec3 lightDir = normalize(light.position - FragPos);
    float dist = length(light.position - FragPos);
    float atten = light.intensity / (1.0 + 0.045 * dist + 0.0075 * dist * dist);

    // ambient
    vec3 ambient = 0.12 * light.color * albedo;

    // diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * light.color * albedo;

    // specular (Blinn-Phong)
    vec3 halfway = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfway), 0.0), uShininess);
    vec3 specular = spec * light.color * 0.5;

    return (ambient + diffuse + specular) * atten;
}

void main() {
    vec3 normal = normalize(Normal);
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 albedo = uUseTexture ? texture(uTexture, UV).rgb : uBaseColor;

    vec3 result = vec3(0.0);
    result += ComputeLight(uLight1, normal, viewDir, albedo);
    result += ComputeLight(uLight2, normal, viewDir, albedo);

    if (uDiscoMode) {
        for (int i = 0; i < uDiscoLightCount; ++i) {
            result += ComputeLight(uDiscoLights[i], normal, viewDir, albedo);
        }
    }

    FragColor = vec4(result, 1.0);
}
