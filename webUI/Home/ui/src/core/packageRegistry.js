function sortPackages(left, right) {
  return (left.order ?? 100) - (right.order ?? 100)
}

const manifestModules = import.meta.glob('../../../../**/bundle.json', {
  eager: true,
  import: 'default',
})

export const featurePackages = Object.values(manifestModules).sort(sortPackages)

export function getPackageStatusTone(status) {
  if (status === 'active') return 'success'
  if (status === 'planned') return 'warning'
  return 'info'
}

export function formatPackageStatus(status) {
  if (status === 'active') return 'Active'
  if (status === 'planned') return 'Planned'
  return 'Pending'
}
