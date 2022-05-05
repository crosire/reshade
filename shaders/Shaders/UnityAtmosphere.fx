/*!
 * (c) 2019 FMS_Cat, MIT License
 * https://opensource.org/licenses/MIT
 *
 * Based on "Accurate Atmospheric Scattering" from GPU Gems2
 * https://developer.nvidia.com/gpugems/GPUGems2/gpugems2_chapter16.html
 *
 * ThreePlanet.js implementation helps me so much
 * https://github.com/idflood/ThreePlanet.js
 */

#define LIGHT_DIR normalize( vec3( -3.0, 3.0, -3.0 ) )
#define CAMERA_HEIGHT 1.000001
#define INV_WAVE_LENGTH vec3( 5.60204474633241, 9.4732844379203038, 19.643802610477206 )
#define INNER_RADIUS 1.0
#define OUTER_RADIUS 1.025
#define ESUN 10.0
#define KR 0.0025
#define KM 0.0015
#define SCALE_DEPTH 0.25
#define SAMPLES 6
#define G -0.99
#define GROUND_COLOR vec3( 0.37, 0.35, 0.34 )
#define GAMMA 1.0 / 2.2

#define PI 3.14159265

mat2 rotate2D( float t ) {
    return mat2( cos( t ), -sin( t ), sin( t ), cos( t ) );
}

float scale( float fCos ) {
    float x = 1.0 - fCos;
    return SCALE_DEPTH * exp( -0.00287 + x * ( 0.459 + x * ( 3.83 + x * ( -6.80 + x * 5.25 ) ) ) );
}

vec2 getIntersections( vec3 pos, vec3 dir, float dist2, float rad2 ) {
    float B = 2.0 * dot( pos, dir );
    float C = dist2 - rad2;
    float det = max( 0.0, B * B - 4.0 * C );
    return 0.5 * vec2(
        ( -B - sqrt( det ) ),
        ( -B + sqrt( det ) )
    );
}

float getRayleighPhase( float fCos2 ) {
    return 0.75 * ( 2.0 + 0.5 * fCos2 );
}

float getMiePhase( float fCos, float fCos2, float g, float g2 ) {
    return 1.5 * ( ( 1.0 - g2 ) / ( 2.0 + g2 ) ) * ( 1.0 + fCos2 )
        / pow( 1.0 + g2 - 2.0 * g * fCos, 1.5 );
}

vec3 uvToRayDir( vec2 uv ) {
    vec2 v = PI * ( vec2( 1.5, 1.0 ) - vec2( 2.0, 1.0 ) * uv );
    return vec3(
        sin( v.y ) * cos( v.x ),
        cos( v.y ),
        sin( v.y ) * sin( v.x )
    );
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Variables
    float fInnerRadius2 = INNER_RADIUS * INNER_RADIUS;
    float fOuterRadius2 = OUTER_RADIUS * OUTER_RADIUS;
    float fKrESun = KR * ESUN;
    float fKmESun = KM * ESUN;
    float fKr4PI = KR * 4.0 * PI;
    float fKm4PI = KM * 4.0 * PI;
    float fScale = 1.0 / ( OUTER_RADIUS - INNER_RADIUS );
    float fScaleOverScaleDepth = fScale / SCALE_DEPTH;
    float fG2 = G * G;

    // Light diection
    vec3 v3LightDir = LIGHT_DIR;
    if ( 0.5 < iMouse.z ) {
		vec2 m = iMouse.xy / iResolution.xy;
        v3LightDir = uvToRayDir( m );
    }

    // Ray initialization
    vec2 v2uv = fragCoord.xy / iResolution.xy;

    vec3 v3RayOri = vec3( 0.0, CAMERA_HEIGHT, 0.0 );
    float fRayPhi = PI * ( 3.0 / 2.0 - 2.0 * v2uv.x );
    float fRayTheta = PI * ( v2uv.y );
    vec3 v3RayDir = vec3(
        sin( fRayTheta ) * cos( fRayPhi ),
        -cos( fRayTheta ),
        sin( fRayTheta ) * sin( fRayPhi )
    );
    float fCameraHeight = length( v3RayOri );
    float fCameraHeight2 = fCameraHeight * fCameraHeight;

    vec2 v2InnerIsects = getIntersections( v3RayOri, v3RayDir, fCameraHeight2, fInnerRadius2 );
    vec2 v2OuterIsects = getIntersections( v3RayOri, v3RayDir, fCameraHeight2, fOuterRadius2 );
    bool isGround = 0.0 < v2InnerIsects.x;

    if ( v2OuterIsects.x == v2OuterIsects.y ) // vacuum space
        {
        fragColor = vec4( 0.0, 0.0, 0.0, 1.0 );
        return;
    }

    float fNear = max( 0.0, v2OuterIsects.x );
    float fFar = isGround ? v2InnerIsects.x : v2OuterIsects.y;
    vec3 v3FarPos = v3RayOri + v3RayDir * fFar;
    vec3 v3FarPosNorm = normalize( v3FarPos );

    vec3 v3StartPos = v3RayOri + v3RayDir * fNear;
    float fStartPosHeight = length( v3StartPos );
    vec3 v3StartPosNorm = v3StartPos / fStartPosHeight;
    float fStartAngle = dot( v3RayDir, v3StartPosNorm );
    float fStartDepth = exp( fScaleOverScaleDepth * ( INNER_RADIUS - fStartPosHeight ) );
    float fStartOffset = fStartDepth * scale( fStartAngle );

    float fCameraAngle = dot( -v3RayDir, v3FarPosNorm );
    float fCameraScale = scale( fCameraAngle );
    float fCameraOffset = exp( ( INNER_RADIUS - fCameraHeight ) / SCALE_DEPTH ) * fCameraScale;

    float fTemp = scale( dot( v3FarPosNorm, v3LightDir ) ) + scale( dot( v3FarPosNorm, -v3RayDir ) );

    float fSampleLength = ( fFar - fNear ) / float( SAMPLES );
    float fScaledLength = fSampleLength * fScale;
    vec3 v3SampleDir = v3RayDir * fSampleLength;
    vec3 v3SamplePoint = v3StartPos + v3SampleDir * 0.5;

    vec3 v3FrontColor = vec3( 0.0 );
    vec3 v3Attenuate;
    for ( int i = 0; i < SAMPLES; i ++ )
        {
        float fHeight = length( v3SamplePoint );
        float fDepth = exp( fScaleOverScaleDepth * ( INNER_RADIUS - fHeight ) );
        float fLightAngle = dot( v3LightDir, v3SamplePoint ) / fHeight;
        float fCameraAngle = dot( v3RayDir, v3SamplePoint ) / fHeight;
        float fScatter = isGround
            ? fDepth * fTemp - fCameraOffset
            : ( fStartOffset + fDepth * ( scale( fLightAngle ) - scale( fCameraAngle ) ) );
        v3Attenuate = exp( -fScatter * ( INV_WAVE_LENGTH * fKr4PI + fKm4PI ) );
        v3FrontColor += v3Attenuate * ( fDepth * fScaledLength );
        v3SamplePoint += v3SampleDir;
    }

    v3FrontColor = clamp( v3FrontColor, 0.0, 3.0 );
    vec3 c0 = v3FrontColor * ( INV_WAVE_LENGTH * fKrESun );
    vec3 c1 = v3FrontColor * fKmESun;

    if ( isGround )
        {
        vec3 v3RayleighColor = c0 + c1;
        vec3 v3MieColor = clamp( v3Attenuate, 0.0, 3.0 );
        fragColor = vec4( 1.0 - exp( -( v3RayleighColor + GROUND_COLOR * v3MieColor ) ), 1.0 );
        fragColor.xyz = pow( fragColor.xyz, vec3( GAMMA ) );
        return;
    }

    float fCos = dot( -v3LightDir, v3RayDir );
    float fCos2 = fCos * fCos;

    fragColor = vec4(
        getRayleighPhase( fCos2 ) * c0 + getMiePhase( fCos, fCos2, G, fG2 ) * c1,
        1.0
    );
    fragColor.xyz = pow( fragColor.xyz, vec3( GAMMA ) );
}
