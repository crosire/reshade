float2x2 swirlTransform(float theta) {
    const float c = cos(theta);
    const float s = sin(theta);

    const float m1 = c;
    const float m2 = -s;
    const float m3 = s;
    const float m4 = c;

    return float2x2(
        m1, m2,
        m3, m4
    );
};

float2x2 zigzagTransform(float theta) {
    const float c = cos(theta);
    return float2x2(
        c, 0,
        0, c
    );
}

float3x3 getrot(float3 r)
{
    const float cx = cos(radians(r.x));
    const float sx = sin(radians(r.x));
    const float cy = cos(radians(r.y));
    const float sy = sin(radians(r.y));
    const float cz = cos(radians(r.z));
    const float sz = sin(radians(r.z));

    const float m1 = cy * cz;
    const float m2= cx * sz + sx * sy * cz;
    const float m3= sx * sz - cx * sy * cz;
    const float m4= -cy * sz;
    const float m5= cx * cz - sx * sy * sz;
    const float m6= sx * cz + cx * sy * sz;
    const float m7= sy;
    const float m8= -sx * cy;
    const float m9= cx * cy;

    return float3x3
    (
        m1,m2,m3,
        m4,m5,m6,
        m7,m8,m9
    );
};