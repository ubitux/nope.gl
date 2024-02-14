import pynopegl as ngl


@ngl.scene()
def animated_scale(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)
    cfg.duration = 3

    animkf = [
        ngl.AnimKeyFrameVec3(0, (1 / 3, 1 / 3, 1 / 3)),
        ngl.AnimKeyFrameVec3(cfg.duration / 2, (3 / 4, 3 / 4, 3 / 4)),
        ngl.AnimKeyFrameVec3(cfg.duration, (1 / 3, 1 / 3, 1 / 3)),
    ]

    scene = ngl.DrawColor(shape=ngl.ShapeCircle(0.5), blending="src_over")
    return ngl.Scale(scene, factors=ngl.AnimatedVec3(animkf))
