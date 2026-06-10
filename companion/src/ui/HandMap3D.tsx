/**
 * 3-D hand model picker — loads public/hand.glb via React Three Fiber.
 *
 * SETUP (one-time):
 *  1. Drop your model at companion/public/hand.glb
 *  2. Run the demo, open the Tune tab, click every finger.
 *     The console prints: [HandMap3D] clicked: "<meshName>"
 *  3. Fill in FINGER_MESH_NAMES below, then set DEBUG_NAMES = false.
 */

import { Canvas } from "@react-three/fiber";
import { OrbitControls, useGLTF, Center, Bounds } from "@react-three/drei";
import { Suspense, useRef, useState } from "react";
import * as THREE from "three";
import type { GLTF } from "three-stdlib";
import { PAD_NAMES } from "../ble/types";

const MODEL_PATH = "/hand.glb";

/**
 * Fill in mesh names after the debug step.
 * Each array can contain multiple mesh names for the same finger.
 */
const FINGER_MESH_NAMES: Record<number, string[]> = {
  0: ["Circle003"],   // Thumb
  1: ["Circle003_1"], // Index
  2: ["Circle003_2"], // Middle
  3: ["Circle003_3"], // Ring
};

/** Click any mesh while this is true to log its name to the console. */
const DEBUG_NAMES = false;

/* ── Colours ──────────────────────────────────────────────────────────── */

/* Skin/palm brightness boost over the raw GLB colour. */
const SKIN_BOOST = 2.6;

/* userData key under which each mesh's pristine GLB colour is stored. We keep
 * it ON the mesh object (not in a module-level Map/WeakMap) because useGLTF
 * caches the scene + meshes in a SEPARATE module that does not reload on HMR,
 * while this module's top-level state IS wiped on every save. A module-level
 * cache would reset on hot-reload, re-read the already-tinted material as the
 * "pristine" value, and compound the boost — bleaching the hand whiter on
 * every edit. mesh.userData rides with the persistent mesh, surviving both
 * component remounts and HMR, so the pristine colour is captured exactly once. */
const PRISTINE_KEY = "__agPristineColor";

/* ── Types ────────────────────────────────────────────────────────────── */
type GLTFResult = GLTF & {
  nodes: Record<string, THREE.Mesh>;
  materials: Record<string, THREE.Material>;
};

interface HandModelProps {
  selected: number;
  hovered: number | null;
  onSelect: (pad: number) => void;
  onHover: (pad: number | null) => void;
}

/* ── Hand mesh component ──────────────────────────────────────────────── */
function HandModel({
  selected,
  hovered,
  onSelect,
  onHover,
}: HandModelProps) {
  const gltf = useGLTF(MODEL_PATH) as GLTFResult;

  // Log all mesh names once on first load
  const loggedRef = useRef(false);
  if (DEBUG_NAMES && !loggedRef.current) {
    loggedRef.current = true;
    const names: string[] = [];
    gltf.scene.traverse((obj) => {
      if (obj instanceof THREE.Mesh) names.push(obj.name);
    });
    console.log("[HandMap3D] all mesh names:", names);
  }

  // Per-mesh cloned material cache (per-instance; safe to reset on remount).
  const matCache = useRef<Map<string, THREE.MeshStandardMaterial>>(new Map());

  /* Pristine colour for a mesh, stored on mesh.userData so it is captured
   * exactly once (the first time the mesh is ever seen, before any tint) and
   * survives both remounts and HMR. */
  function getPristine(mesh: THREE.Mesh): THREE.Color {
    let c = mesh.userData[PRISTINE_KEY] as THREE.Color | undefined;
    if (!c) {
      const src = Array.isArray(mesh.material) ? mesh.material[0] : mesh.material;
      c = src instanceof THREE.MeshStandardMaterial
        ? src.color.clone()
        : new THREE.Color(1, 1, 1);
      mesh.userData[PRISTINE_KEY] = c;
    }
    return c;
  }

  function getMat(mesh: THREE.Mesh): THREE.MeshStandardMaterial {
    if (!matCache.current.has(mesh.uuid)) {
      const src = Array.isArray(mesh.material)
        ? mesh.material[0]
        : mesh.material;
      const mat = src instanceof THREE.MeshStandardMaterial
        ? src.clone()
        : new THREE.MeshStandardMaterial();
      mesh.material = mat;
      matCache.current.set(mesh.uuid, mat);
    }
    return matCache.current.get(mesh.uuid)!;
  }

  function padForMesh(name: string): number {
    for (const [padStr, names] of Object.entries(FINGER_MESH_NAMES)) {
      if (names.includes(name)) return Number(padStr);
    }
    return -1;
  }

  // Collect all meshes
  const meshes: THREE.Mesh[] = [];
  gltf.scene.traverse((obj) => {
    if (obj instanceof THREE.Mesh) meshes.push(obj);
  });

  /* Apply per-finger tinting every render. Every branch starts from the
   * pristine GLB colour (copy, not multiply-in-place), so the result is fully
   * idempotent — re-running this loop any number of times yields the same
   * colour and can never accumulate. */
  for (const mesh of meshes) {
    const pad = padForMesh(mesh.name);
    const mat = getMat(mesh);
    const orig = getPristine(mesh);

    if (pad < 0) {
      // Skin/palm/wrist — fixed brightness boost, no glow.
      mat.color.copy(orig).multiplyScalar(SKIN_BOOST);
      mat.emissive.set(0, 0, 0);
      mat.emissiveIntensity = 0;
      mesh.material = mat;
      continue;
    }

    const isSel = selected === pad;
    const isHov = hovered === pad;

    if (isSel) {
      // Selected — strong brighten + bold emissive glow
      mat.color.copy(orig).multiplyScalar(2.0);
      mat.emissive.copy(orig).multiplyScalar(0.6);
      mat.emissiveIntensity = 1.0;
    } else if (isHov) {
      // Hover — clearly lighter with visible glow
      mat.color.copy(orig).multiplyScalar(1.8);
      mat.emissive.copy(orig).multiplyScalar(0.3);
      mat.emissiveIntensity = 0.6;
    } else {
      // Idle — restore original GLB baked colour
      mat.color.copy(orig);
      mat.emissive.set(0, 0, 0);
      mat.emissiveIntensity = 0;
    }
    mesh.material = mat;
  }

  return (
    <Center>
        <primitive
        object={gltf.scene}
        scale={[-1, 1, 1]}
        onClick={(e: { stopPropagation: () => void; object: unknown }) => {
          e.stopPropagation();
          const mesh = e.object as THREE.Mesh;
          if (DEBUG_NAMES) {
            console.log(`[HandMap3D] clicked: "${mesh.name}"`);
          }
          const pad = padForMesh(mesh.name);
          if (pad >= 0) onSelect(pad);
        }}
        onPointerEnter={(e: { stopPropagation: () => void; object: unknown }) => {
          e.stopPropagation();
          const pad = padForMesh((e.object as THREE.Mesh).name);
          onHover(pad >= 0 ? pad : null);
        }}
        onPointerLeave={(e: { stopPropagation: () => void }) => {
          e.stopPropagation();
          onHover(null);
        }}
      />
    </Center>
  );
}

/* ── Loading spinner ──────────────────────────────────────────────────── */
function LoadingRing() {
  return (
    <mesh rotation={[0, 0, 0]}>
      <torusGeometry args={[0.6, 0.08, 8, 32]} />
      <meshBasicMaterial color="#5e6ad2" wireframe />
    </mesh>
  );
}

/* ── Scene ────────────────────────────────────────────────────────────── */
interface SceneProps {
  visualSel: number | null;
  onSelect: (pad: number) => void;
}

function HandScene({ visualSel, onSelect }: SceneProps) {
  const [hovered, setHovered] = useState<number | null>(null);
  return (
    <>
      {/* Soft, studio-style lighting for the light background */}
      <ambientLight intensity={0.75} />
      <directionalLight position={[3, 6, 4]} intensity={0.7} />
      <directionalLight position={[-4, 2, 2]} intensity={0.35} color="#c8d0ff" />
      <directionalLight position={[0, -2, 3]} intensity={0.15} />

      <Bounds fit clip observe margin={0.88}>
        <Suspense fallback={<LoadingRing />}>
          <HandModel
            selected={visualSel ?? -1}
            hovered={hovered}
            onSelect={onSelect}
            onHover={setHovered}
          />
        </Suspense>
      </Bounds>

      <OrbitControls
        makeDefault
        enableZoom={false}
        enablePan={false}
        minPolarAngle={Math.PI * 0.15}
        maxPolarAngle={Math.PI * 0.75}
      />

    </>
  );
}

/* ── Public component ─────────────────────────────────────────────────── */
export interface HandMap3DProps {
  selected: number;
  onSelect: (pad: number) => void;
  /** Kept for API compat — no longer used by the 3D view. */
  touch?: unknown;
  thresholds?: unknown;
  actions?: unknown;
}

export function HandMap3D({ selected, onSelect }: HandMap3DProps) {
  const [visualSel, setVisualSel] = useState<number | null>(selected);

  function handleSelect(pad: number) {
    setVisualSel(pad);
    onSelect(pad);
  }

  return (
    <div className="hand3d-root">
      <div className="hand3d-canvas">
        <Canvas
          camera={{ position: [0, 0, 5], fov: 45 }}
          gl={{ antialias: true, alpha: true }}
          style={{ background: "transparent" }}
          onPointerMissed={() => setVisualSel(null)}
        >
          <HandScene
            visualSel={visualSel}
            onSelect={handleSelect}
          />
        </Canvas>
      </div>

      {/* Finger tab-strip */}
      <div className="hand3d-tabs" role="tablist">
        {PAD_NAMES.map((name, i) => (
          <button
            key={name}
            role="tab"
            aria-selected={selected === i}
            className={`hand3d-tab${selected === i ? " hand3d-tab--active" : ""}`}
            onClick={() => onSelect(i)}
          >
            {name}
          </button>
        ))}
      </div>
    </div>
  );
}

useGLTF.preload(MODEL_PATH);
