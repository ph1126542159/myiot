function sortPackages(left, right) {
  return (left.order ?? 100) - (right.order ?? 100)
}

const manifestModules = import.meta.glob('../../../../**/bundle.json', {
  eager: true,
  import: 'default',
})

export const featurePackages = Object.values(manifestModules).sort(sortPackages)
