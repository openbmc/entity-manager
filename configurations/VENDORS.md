# Vendors

To simplify the organization and ownership of configuration files, they can be
organized into vendor-specific subdirectories. This begs the question "what is a
vendor"?

In some cases a company might design a component (such as a network card),
manufacture said component, and package it into an elegant cardboard box with
their logo, which is sold on retail store shelves to customers; this company is
clearly "the vendor" of said component. Where the situation is less clear is
when multiple companies are involved in the chain between design and end-user.

For purposes of this repository, the following prioritized guidelines are used
for identifying the vendor:

1. A company which primarily initiates and oversees the design, manufacture and
   sale of a component is always the vendor. "Sale" does not require retail but
   can include components which are exclusively sold to other enterprises for
   inclusion in their products. This covers typical "components" such as network
   cards, processors, and mainboards.

2. When a company is assembling a group of components into a single product,
   which they exclusively label and sell, such as a server chassis, they are the
   vendor for the assembled product and any sub-components which are exclusively
   designed for and used by their assembled product(s). Sub-components that are
   procured from others and may be sold by others for use in other products
   should be covered by guideline (1).

3. When one company primarily oversees the design but other companies
   manufacture and/or sell the component or system, such as [OCP][OCP] designs,
   the company that primarily oversaw the design of the component would be the
   vendor.

These guidelines are not meant to be exhaustive rules to cover all scenarios and
contractual arrangements, but simply direction pointing for how the repository
is intended to be organized. The overriding principle should be: if someone
claims to be the vendor of a device, they probably are, unless there is strong
evidence they are not.

[OCP]: https://www.opencompute.org/
