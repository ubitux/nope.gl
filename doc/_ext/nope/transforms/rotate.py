import pynopegl as ngl


@ngl.scene()
def rotate(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (1, 1)

    scene = ngl.DrawColor(box=(-0.5, -0, 5, 1, 1))
    return ngl.Rotate(scene, angle=45)
