========================================
gl3w: Simple OpenGL core profile loading
========================================

Introduction
------------

gl3w_ is the easiest way to get your hands on the functionality offered by the
OpenGL core profile specification.

Its main part is a simple gl3w_gen.py_ Python script that downloads the
Khronos_ supported glcorearb.h_ header and generates gl3w.h and gl3w.c from it.
Those files can then be added and linked (statically or dynamically) into your
project.

Example
-------

Here is a simple example of using gl3w_ with glut. Note that GL/gl3w.h must be
included before any other OpenGL related headers::

    #include <stdio.h>
    #include <GL/gl3w.h>
    #include <GL/glut.h>

    // ...

    int main(int argc, char **argv)
    {
            glutInit(&argc, argv);
            glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE);
            glutInitWindowSize(width, height);
            glutCreateWindow("cookie");

            glutReshapeFunc(reshape);
            glutDisplayFunc(display);
            glutKeyboardFunc(keyboard);
            glutSpecialFunc(special);
            glutMouseFunc(mouse);
            glutMotionFunc(motion);

            if (gl3wInit()) {
                    fprintf(stderr, "failed to initialize OpenGL\n");
                    return -1;
            }
            if (!gl3wIsSupported(3, 2)) {
                    fprintf(stderr, "OpenGL 3.2 not supported\n");
                    return -1;
            }
            printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION),
                   glGetString(GL_SHADING_LANGUAGE_VERSION));

            // ...

            glutMainLoop();
            return 0;
    }

API Reference
-------------

The gl3w_ API consists of just three functions:

``int gl3wInit(void)``

    Initializes the library. Should be called once after an OpenGL context has
    been created. Returns ``0`` when gl3w_ was initialized successfully,
    ``-1`` if there was an error.

``int gl3wIsSupported(int major, int minor)``

    Returns ``1`` when OpenGL core profile version *major.minor* is available
    and ``0`` otherwise.

``GL3WglProc gl3wGetProcAddress(const char *proc)``

    Returns the address of an OpenGL extension function. Generally, you won't
    need to use it since gl3w_ loads all functions defined in the OpenGL core
    profile on initialization. It allows you to load OpenGL extensions outside
    of the core profile.

License
-------

gl3w_ is in the public domain.

Credits
-------

Slavomir Kaslev <slavomir.kaslev@gmail.com>
    Initial implementation

Kelvin McDowell
    Mac OS X support

Sjors Gielen
    Mac OS X support

Travis Gesslein
    Patches regarding glcorearb.h

Rommel160 [github.com/Rommel160]
    Code contributions

Copyright
---------

OpenGL_ is a registered trademark of SGI_.

.. _gl3w: https://github.com/skaslev/gl3w
.. _gl3w_gen.py: https://github.com/skaslev/gl3w/blob/master/gl3w_gen.py
.. _glcorearb.h: http://www.opengl.org/registry/api/glcorearb.h
.. _OpenGL: http://www.opengl.org/
.. _Khronos: http://www.khronos.org/
.. _SGI: http://www.sgi.com/
