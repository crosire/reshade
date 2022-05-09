precision highp float;
uniform float time;
uniform vec2 resolution;
varying vec3 fPosition;
varying vec3 fNormal;

void main()
{
  vec3 normal = normalize(fNormal);
  
  vec3 lightDir = normalize(vec3(1,1,0.8));
  vec3 viewDir = vec3(0,0,1);
  float lambert = clamp(dot(lightDir, normal),0.0,1.0);
  float blinn = clamp(dot(normalize(lightDir+viewDir),normal),0.0,1.0);
  float fresnel = dot(viewDir, normalize(fNormal));
  float IOR = 1.46; // plastic
  float roughness = 0.001;
  
  float reflectivity = (1.0 - IOR) / (1.0 + IOR);
  reflectivity *= reflectivity;
  
  float schlick = reflectivity + (1.0 - reflectivity) * pow(1.0 - fresnel, 5.0);
  
  lambert = mix(lambert, 1.0, 0.13);
  
  vec3 diff = vec3(0.08,0.1,0.2)*lambert;
  diff *= 0.5; // integrate(cos(x), x, -pi/2, pi/2) == 2

  // spec integral from -pi/2 to pi/2:
  // s*((4*sqrt(2*s-s^2)*atan(sqrt(2*s-s^2)/s))/(s^3-3*s^2+2*s)+(2*%pi*s)/(2*s^2-6*s+4)-(2*%pi)/(s^2-3*s+2))

  float spec_integral = pow(roughness, 0.264) * 2.0; // approximation

  float specCoef = roughness / (roughness + 1.0/sqrt(blinn) - 1.0);
  specCoef /= spec_integral;
  
  int GGX = 0;  // toggle for comparison
  
  if (GGX != 0)  
  {
          // Trowbridge-Reitz (for comparison)
          // Walter et al. 2007, "Microfacet models for refraction through rough surfaces"
          // http://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
          
          float a = 0.1;
          float a2 = a*a;
          specCoef = blinn * blinn * (a2 - 1.0) + 1.0;
          specCoef = 3.1415 * specCoef * specCoef;
          specCoef = a2 / specCoef;
          
          specCoef *= 0.1; // match brightness
  }
  
  vec3 spec = vec3(1,1,1)*specCoef;
  
  vec3 color = mix(diff, spec, schlick);
  color *= 2.5; // brighten
  
  int specular_only = 0;  // disable to view diffuse + specular
  
  if (specular_only != 0)
  {
    color = spec*schlick*4.0;
  }
  
  color *= 4.0;
  
  gl_FragColor = vec4(color, 1.0);
}
