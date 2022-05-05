//Adjust these values as you like:

#define speed 0.01 //How fast the stripes move along the ground
#define stripewidth 100 //How large the stripes are (smaller value = bigger stripes)

#define shakespeed 0.01 //How fast the screen "shakes"
#define shakeamount 0.02 //How strong the "shake" effect is


uniform int framecount < source = "framecount"; >;

float3 CustomPass(float4 position : SV_Position, float2 texcoord : TEXCOORD0) : SV_Target
{

  float frames = framecount * 1.0;
  float2 texShake = texcoord * 1.0;
  texShake.x += sin(frames * shakespeed) * shakeamount;
  texShake.y += cos(frames * shakespeed) * shakeamount;

  float3 colorInput = tex2D(s0, texShake).rgb;
  float depth = tex2D(depthSampler, texcoord).x;

  depth = (2.0 * Depth_z_near) / ( -(Depth_z_far - Depth_z_near) * depth + (Depth_z_far + Depth_z_near) );  

  colorInput.r = colorInput.r + position.x / BUFFER_WIDTH - abs(cos(frames * speed) + sin(depth * stripewidth));
  colorInput.g = colorInput.g + position.y / BUFFER_HEIGHT - abs(sin(frames * speed) + cos(depth * stripewidth));


  return saturate(colorInput);
}