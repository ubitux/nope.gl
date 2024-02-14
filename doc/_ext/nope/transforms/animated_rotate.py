import pynopegl as ngl


@ngl.scene()
def animated_rotate(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 3

    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 360),
    ]

    scene = ngl.DrawColor(shape=ngl.ShapeRectangle(size=(0.5, 0.5)), blending="src_over")
    return ngl.Rotate(scene, angle=ngl.AnimatedFloat(animkf))
