import array
import pynodegl as ngl

from pynodegl_utils.misc import scene


@scene()
def shape_0_triangle(cfg):
    """Rendering a triangle"""

    cfg.aspect_ratio = (1, 1)
    return ngl.RenderColor(geometry=ngl.Triangle())


@scene()
def shape_1_quad(cfg):
    """Rendering a quadrilateral"""

    cfg.aspect_ratio = (1, 1)
    return ngl.RenderColor(geometry=ngl.Quad())


@scene()
def shape_2_circle(cfg):
    """Rendering a circle with 64 points"""

    cfg.aspect_ratio = (1, 1)
    return ngl.RenderColor(geometry=ngl.Circle(radius=0.7, npoints=64))


@scene()
def shape_3_geometry(cfg):
    """Rendering a custom geometry composed of 4 random points"""

    cfg.aspect_ratio = (1, 1)

    p0 = cfg.rng.uniform(-1, 0), cfg.rng.uniform(0, 1), 0.0
    p1 = cfg.rng.uniform(0, 1), cfg.rng.uniform(0, 1), 0.0
    p2 = cfg.rng.uniform(-1, 0), cfg.rng.uniform(-1, 0), 0.0
    p3 = cfg.rng.uniform(0, 1), cfg.rng.uniform(-1, 0), 0.0

    vertices = array.array("f", p0 + p1 + p2 + p3)
    uvcoords = array.array("f", p0[:2] + p1[:2] + p2[:2] + p3[:2])

    geometry = ngl.Geometry(
        vertices=ngl.BufferVec3(data=vertices),
        uvcoords=ngl.BufferVec2(data=uvcoords),
        topology="triangle_strip",
    )

    return ngl.RenderColor(geometry=geometry)


@scene()
def transform_0_rotate(cfg):
    """Rotate a quad by 45°"""

    cfg.aspect_ratio = (1, 1)

    scene = ngl.RenderColor(geometry=ngl.Quad())
    return ngl.Rotate(scene, angle=45)


@scene()
def transform_1_animated_rotate(cfg):
    """Rotate a quad by 360° over the course of 3 seconds"""

    cfg.aspect_ratio = (1, 1)
    cfg.duration = 3

    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 360)
    ]

    scene = ngl.RenderColor(geometry=ngl.Quad())
    return ngl.Rotate(scene, angle=ngl.AnimatedFloat(animkf))


@scene()
def transform_2_animated_translate(cfg):
    """Translate a circle from one side to another, and back"""

    cfg.aspect_ratio = (1, 1)
    cfg.duration = 3

    animkf = [
        ngl.AnimKeyFrameVec3(0, (-1 / 3, 0, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration / 2, (1 / 3, 0, 0)),
        ngl.AnimKeyFrameVec3(cfg.duration, (-1 / 3, 0, 0)),
    ]

    scene = ngl.RenderColor(geometry=ngl.Circle(0.5))
    return ngl.Translate(scene, vector=ngl.AnimatedVec3(animkf))


@scene()
def transform_3_animated_scale(cfg):
    """Scale up and down a circle"""

    cfg.aspect_ratio = (1, 1)
    cfg.duration = 3

    animkf = [
        ngl.AnimKeyFrameVec3(0, (1 / 3, 1 / 3, 1)),
        ngl.AnimKeyFrameVec3(cfg.duration / 2, (3 / 4, 3 / 4, 1)),
        ngl.AnimKeyFrameVec3(cfg.duration, (1 / 3, 1 / 3, 1)),
    ]

    scene = ngl.RenderColor(geometry=ngl.Circle(0.5))
    return ngl.Scale(scene, factors=ngl.AnimatedVec3(animkf))


@scene()
def render_0_gradient(cfg):
    """A simple horizontal gradient from teal to orange"""

    return ngl.RenderGradient(
        color0=(0, 0.5, 1),
        color1=(1, 0.5, 0),
    )


@scene()
def render_1_image(cfg):
    """Render an image"""

    image = next(m for m in cfg.medias if m.filename.endswith(".jpg"))

    cfg.aspect_ratio = image.width, image.height

    # Warning: the texture can be shared, but not the media
    media = ngl.Media(image.filename)
    tex = ngl.Texture2D(data_src=media)
    return ngl.RenderTexture(tex)


@scene()
def render_2_video(cfg):
    """Render a video"""

    video = next(m for m in cfg.medias if m.filename.endswith(".mp4"))

    cfg.aspect_ratio = video.width, video.height
    cfg.duration = video.duration

    # Warning: the texture can be shared, but not the media
    media = ngl.Media(video.filename)
    tex = ngl.Texture2D(data_src=media)
    return ngl.RenderTexture(tex)


@scene()
def blending_0_fade(cfg):
    """Fade in and out as JPEG over another"""

    jpegs = (m for m in cfg.medias if m.filename.endswith(".jpg"))
    image0 = next(jpegs)
    image1 = next(jpegs)

    cfg.aspect_ratio = image0.width, image0.height
    cfg.duration = 3

    bg_tex = ngl.Texture2D(data_src=ngl.Media(image0.filename))
    bg = ngl.RenderTexture(bg_tex)

    fg_tex = ngl.Texture2D(data_src=ngl.Media(image1.filename))
    fg = ngl.RenderTexture(fg_tex)
    fg.set_blending("src_over")

    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration / 2, 1),
        ngl.AnimKeyFrameFloat(cfg.duration, 0),
    ]

    fg.add_filters(
        ngl.FilterOpacity(ngl.AnimatedFloat(animkf)),
    )

    return ngl.Group(children=(bg, fg))


@scene()
def blending_1_overlay(cfg):
    """Overlay a transparent PNG over an image"""

    image = next(m for m in cfg.medias if m.filename.endswith(".jpg"))
    overlay = next(m for m in cfg.medias if m.filename.endswith(".png"))

    cfg.aspect_ratio = image.width, image.height

    bg_tex = ngl.Texture2D(data_src=ngl.Media(image.filename))
    bg = ngl.RenderTexture(bg_tex)

    fg_tex = ngl.Texture2D(data_src=ngl.Media(overlay.filename))
    fg = ngl.RenderTexture(fg_tex)
    fg.set_blending("src_over")

    fg.add_filters(
        # A PNG is usually not premultiplied, but the blending operator expects
        # it to be.
        ngl.FilterPremult(),
        # Make the overlay half opaque
        ngl.FilterOpacity(0.5),
    )

    return ngl.Group(children=(bg, fg))


@scene()
def timerange_0_elems(cfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 6.0

    # Draw the same shape 3 times in a different place
    shape = ngl.Scale(ngl.RenderColor(), factors=(1/6, 1/6, 1))
    shape_x3 = [ngl.Translate(shape, (x, 0, 0)) for x in (-0.5, 0, 0.5)]

    # Define a different time range for each branch
    ranges = [
        [ngl.TimeRangeModeNoop(0), ngl.TimeRangeModeCont(1), ngl.TimeRangeModeNoop(4)],
        [ngl.TimeRangeModeNoop(0), ngl.TimeRangeModeCont(2), ngl.TimeRangeModeNoop(5)],
        [ngl.TimeRangeModeNoop(0), ngl.TimeRangeModeCont(3), ngl.TimeRangeModeNoop(6)],
    ]
    range_filters = [ngl.TimeRangeFilter(s, r) for s, r in zip(shape_x3, ranges)]
    return ngl.Group(range_filters)
