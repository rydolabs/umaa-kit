declare namespace React {
  class Component<P = {}, S = {}> {
    props: P; state: S;
    constructor(props: P);
    setState(update: Partial<S> | ((prev: S) => Partial<S>)): void;
    render(): any;
  }
  function createElement(type: any, props: any, ...children: any[]): any;
}
declare const ReactDOM: { render(component: any, element: Element | null): void };
declare namespace JSX { interface IntrinsicElements { [elemName: string]: any } }
