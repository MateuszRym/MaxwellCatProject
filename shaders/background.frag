#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform float uTime;
uniform bool uDiscoMode;
uniform vec3 uBaseColor;     // domyslny kolor szaro-zielony
uniform float uDiscoBlend;   // 0..1 plynne przejscie do trybu disco

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    // plynnie plynacy hue w oparciu o czas i pozycje ekranu (efekt "tecza")
    float hue = fract(uTime * 0.15 + vUV.x * 0.35 + vUV.y * 0.2);
    vec3 rainbow = hsv2rgb(vec3(hue, 0.85, 0.95));

    vec3 finalColor = mix(uBaseColor, rainbow, uDiscoBlend);
    FragColor = vec4(finalColor, 1.0);
}
