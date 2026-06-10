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


const C_EMISSIVE_SEL = new THREE.Color("#1a1860"); // deep glow when selected
const C_TOUCH_GLOW   = new THREE.Color("#a8ffdc"); // green-white when pad fires

/* ── Types ────────────────────────────────────────────────────────────── */
type GLTFResult = GLTF & {
  nodes: Record<string, THREE.Mesh>;
  materials: Record<string, THREE.Material>;
};

interface HandModelProps {
  selected: number;
  hovered: number | null;
  touch: [number, number, number, number];
  thresholds: [number, number, number, number];
  onSelect: (pad: number) => void;
  onHover: (pad: number | null) => void;
}

/* ── Hand mesh component ──────────────────────────────────────────────── */
function HandModel({
  selected,
  hovered,
  touch,
  thresholds,
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

  // Per-mesh cloned material + original colour cache
  const matCache = useRef<Map<string, THREE.MeshStandardMaterial>>(new Map());
  const origColor = useRef<Map<string, THREE.Color>>(new Map());

  function getMat(mesh: THREE.Mesh): THREE.MeshStandardMaterial {
    if (!matCache.current.has(mesh.uuid)) {
      const src = Array.isArray(mesh.material)
        ? mesh.material[0]
        : mesh.material;
      const mat = src instanceof THREE.MeshStandardMaterial
        ? src.clone()
        : new THREE.MeshStandardMaterial();
      matCache.current.set(mesh.uuid, mat);
      origColor.current.set(mesh.uuid, mat.color.clone());
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

  // Apply per-finger tinting every render
  for (const mesh of meshes) {
    const pad = padForMesh(mesh.name);

    // Brighten skin/palm/wrist meshes (not pad zones, not the outline)
    if (pad < 0) {
      const mat = getMat(mesh);
      const orig = origColor.current.get(mesh.uuid);
      if (orig) mat.color.copy(orig).multiplyScalar(1.55);
      mat.emissive.set(0, 0, 0);
      mat.emissiveIntensity = 0;
      mesh.material = mat;
      continue;
    }
    const mat = getMat(mesh);
    const isSel = selected === pad;
    const isHov = hovered === pad;
    const live = touch[pad] ?? 0;
    const thr = thresholds[pad] ?? 600;

    if (live > thr) {
      // Touch firing — vivid glow regardless of selection state
      mat.color.copy(C_TOUCH_GLOW);
      mat.emissive.copy(C_TOUCH_GLOW);
      mat.emissiveIntensity = Math.min(((live / thr) - 1) * 0.7, 0.55);
    } else if (isSel) {
      // Selected — strong brighten + bold emissive glow
      const orig = origColor.current.get(mesh.uuid);
      if (orig) mat.color.copy(orig).multiplyScalar(2.0);
      mat.emissive.copy(orig ?? C_EMISSIVE_SEL).multiplyScalar(0.6);
      mat.emissiveIntensity = 1.0;
    } else if (isHov) {
      // Hover — clearly lighter with visible glow
      const orig = origColor.current.get(mesh.uuid);
      if (orig) mat.color.copy(orig).multiplyScalar(1.8);
      mat.emissive.copy(orig ?? mat.color).multiplyScalar(0.3);
      mat.emissiveIntensity = 0.6;
    } else {
      // Idle — restore original GLB baked colour
      const orig = origColor.current.get(mesh.uuid);
      if (orig) mat.color.copy(orig);
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
  selected: number;
  onSelect: (pad: number) => void;
  touch: [number, number, number, number];
  thresholds: [number, number, number, number];
}

function HandScene({ selected, onSelect, touch, thresholds }: SceneProps) {
  const [hovered, setHovered] = useState<number | null>(null);
  return (
    <>
      {/* Soft, studio-style lighting for the light background */}
      <ambientLight intensity={1.1} />
      <directionalLight position={[3, 6, 4]} intensity={0.9} />
      <directionalLight position={[-4, 2, 2]} intensity={0.4} color="#c8d0ff" />
      <directionalLight position={[0, -2, 3]} intensity={0.2} />

      <Bounds fit clip observe margin={1.1}>
        <Suspense fallback={<LoadingRing />}>
          <HandModel
            selected={selected}
            hovered={hovered}
            touch={touch}
            thresholds={thresholds}
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
  touch?: [number, number, number, number];
  thresholds?: [number, number, number, number];
  /** Unused in the 3D view but kept for API compat with the flat version. */
  actions?: unknown;
}

export function HandMap3D({
  selected,
  onSelect,
  touch = [0, 0, 0, 0],
  thresholds = [600, 600, 600, 600],
}: HandMap3DProps) {
  return (
    <div className="hand3d-root">
      <div className="hand3d-canvas">
        <Canvas
          camera={{ position: [0, 0, 5], fov: 45 }}
          gl={{ antialias: true, alpha: true }}
          style={{ background: "transparent" }}
        >
          <HandScene
            selected={selected}
            onSelect={onSelect}
            touch={touch}
            thresholds={thresholds}
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
